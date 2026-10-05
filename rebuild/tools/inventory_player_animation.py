#!/usr/bin/env python3
"""Inventory installed player knife animation states, clips, and authored events.

Read-only inputs; no Unity/game process, rendering, downloads, or desktop input.
Requires optional UnityPy (tested 1.25.4). By default writes only an ignored JSON
report under rebuild/build. --raw-dir additionally writes selected clip type
trees for future decoder work; it does not pretend to sample humanoid poses.
The installed sharedassets1 file contains FemaleAnimator2, its MaleAnimator
override, and both avatars. File/object IDs are retained as provenance.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STATES = {"Attack6", "Attack7", "Attack8", "Attack9", "RunSwordAttack",
          "SprintSwordAttack", "airattack0", "airattack1", "WaterSwordAttack0",
          "WaterSwordAttack1", "WaterSwordAttack2", "AxPutOn", "AxPutOff"}


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def clip_info(path_id: int, data: dict) -> dict:
    muscle = data["m_MuscleClip"]
    packed = muscle["m_Clip"]["data"]
    bindings = data["m_ClipBindingConstant"]["genericBindings"]
    dense = packed["m_DenseClip"]
    return {
        "path_id": path_id, "name": data["m_Name"],
        "sample_rate": data["m_SampleRate"],
        "start_time": muscle["m_StartTime"], "stop_time": muscle["m_StopTime"],
        "events": [{"time": row["time"], "function": row["functionName"],
                    "data": row["data"]} for row in data["m_Events"]],
        "ordinary_curve_counts": {k: len(v) for k, v in data.items()
                                  if "Curves" in k and isinstance(v, list)},
        "packed_curves": {
            "streamed_curves": packed["m_StreamedClip"]["curveCount"],
            "dense_frames": dense["m_FrameCount"],
            "dense_curves": dense["m_CurveCount"],
            "dense_sample_rate": dense["m_SampleRate"],
            "constant_values": len(packed["m_ConstantClip"]["data"]),
            "transform_bindings": sum(b["classID"] == 4 for b in bindings),
            "humanoid_bindings": sum(b["classID"] == 95 and b["customType"] == 8 for b in bindings),
        },
        "generic_bindings": bindings,
        "root_motion": {k: muscle[k] for k in ("m_StartX", "m_AverageSpeed", "m_LoopTime",
                                               "m_KeepOriginalOrientation", "m_KeepOriginalPositionY",
                                               "m_KeepOriginalPositionXZ")},
    }


def prefab_mounts(UnityPy, path: Path) -> dict:
    env = UnityPy.load(str(path))
    names = {obj.path_id: obj.read_typetree()["m_Name"] for obj in env.objects
             if obj.type.name == "GameObject"}
    transforms = {obj.path_id: obj.read_typetree() for obj in env.objects
                  if obj.type.name == "Transform"}
    def chain(path_id: int) -> list[dict]:
        result, visited = [], set()
        while path_id:
            if path_id in visited or path_id not in transforms:
                raise ValueError(f"Invalid transform ancestry in {path}")
            visited.add(path_id)
            row = transforms[path_id]
            result.append({"path_id": path_id, "name": names[row["m_GameObject"]["m_PathID"]],
                           "position": row["m_LocalPosition"], "rotation": row["m_LocalRotation"],
                           "scale": row["m_LocalScale"]})
            path_id = row["m_Father"]["m_PathID"]
        return list(reversed(result))
    selected = [chain(i) for i, row in transforms.items()
                if names[row["m_GameObject"]["m_PathID"]] in ("mountMain", "mountWaist", "SingleHandSword")]
    return {"path": str(path.relative_to(ROOT)), "sha256": digest(path), "mount_chains": selected}


def inventory(UnityPy, asset: Path, raw_dir: Path | None) -> dict:
    env = UnityPy.load(str(asset))
    serialized = next(iter(env.files.values()))
    controllers = {}
    for obj in env.objects:
        if obj.type.name in ("AnimatorController", "AnimatorOverrideController"):
            row = obj.read_typetree()
            controllers[row["m_Name"]] = (obj.path_id, row)
    controller_id, controller = controllers["FemaleAnimator2"]
    override_id, override = controllers["MaleAnimator"]
    if override["m_Controller"] != {"m_FileID": 0, "m_PathID": controller_id}:
        raise ValueError("Male override no longer references expected controller")
    names = dict(controller["m_TOS"])
    overrides = {x["m_OriginalClip"]["m_PathID"]: x["m_OverrideClip"]["m_PathID"]
                 for x in override["m_Clips"]}
    references, selected, clip_ids = controller["m_AnimationClips"], [], set()
    for machine_index, wrapped_machine in enumerate(controller["m_Controller"]["m_StateMachineArray"]):
        machine = wrapped_machine["data"]
        for state_index, wrapped_state in enumerate(machine["m_StateConstantArray"]):
            state = wrapped_state["data"]
            name = names.get(state["m_NameID"], "")
            if name not in STATES:
                continue
            nodes = []
            for tree in state["m_BlendTreeConstantArray"]:
                for wrapped_node in tree["data"]["m_NodeArray"]:
                    node = wrapped_node["data"]
                    row = {"raw": node, "blend_parameter": names.get(node["m_BlendEventID"])}
                    index = node["m_ClipID"]
                    if index < len(references):
                        ref = references[index]
                        if ref["m_FileID"]:
                            raise ValueError("Expected local animation clip")
                        female = ref["m_PathID"]
                        male = overrides.get(female, female)
                        row.update(female_clip=female, male_clip=male)
                        clip_ids.update((female, male))
                    nodes.append(row)
            incoming = []
            for wrapped_transition in machine["m_AnyStateTransitionConstantArray"]:
                transition = wrapped_transition["data"]
                if transition["m_DestinationState"] == state_index:
                    incoming.append({"raw": transition, "conditions": [
                        {"parameter": names.get(c["data"]["m_EventID"]), **c["data"]}
                        for c in transition["m_ConditionConstantArray"]]})
            selected.append({"name": name, "machine_index": machine_index, "state_index": state_index,
                             "full_path": names.get(state["m_FullPathID"]), "raw_state": state,
                             "nodes": nodes, "incoming_any_state_transitions": incoming})
    if not {"Attack6", "Attack7", "Attack8", "Attack9"}.issubset(x["name"] for x in selected):
        raise ValueError("Required knife states missing")
    clips = []
    if raw_dir:
        raw_dir.mkdir(parents=True, exist_ok=True)
    for path_id in sorted(clip_ids):
        data = serialized.objects[path_id].read_typetree()
        clips.append(clip_info(path_id, data))
        if raw_dir:
            (raw_dir / f"clip-{path_id}.json").write_text(json.dumps(data, indent=2)+"\n")
    avatars = []
    for obj in env.objects:
        if obj.type.name == "Avatar":
            data = obj.read_typetree()
            avatars.append({"path_id": obj.path_id, "name": data["m_Name"],
                            "path_names": data["m_TOS"], "raw_avatar": data["m_Avatar"]})
    return {"schema": 1, "asset": str(asset), "asset_sha256": digest(asset),
            "unity_version": serialized.unity_version,
            "controller": {"name": controller["m_Name"], "path_id": controller_id},
            "male_override": {"name": override["m_Name"], "path_id": override_id},
            "states": selected, "clips": clips, "avatars": avatars}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", type=Path,
                        default=Path.home()/".local/share/Steam/steamapps/common/Planet Explorers")
    parser.add_argument("--output", type=Path, default=ROOT/"rebuild/build/player-animation.json")
    parser.add_argument("--raw-dir", type=Path, help="Optional directory for selected raw clip type trees")
    args = parser.parse_args()
    try:
        import UnityPy
    except ImportError:
        parser.error("Optional UnityPy is required (tested 1.25.4); use an isolated Python environment")
    asset = args.game_dir.expanduser().resolve()/"PE_Client_Data/sharedassets1.assets"
    output = args.output.expanduser().resolve()
    raw_dir = args.raw_dir.expanduser().resolve() if args.raw_dir else None
    game_dir = args.game_dir.expanduser().resolve()
    for destination in (output, raw_dir):
        if destination and (destination == game_dir or game_dir in destination.parents):
            parser.error("Output must not modify the installed game directory")
    try:
        report = inventory(UnityPy, asset, raw_dir)
        report["source_mounts"] = [prefab_mounts(UnityPy, ROOT/path) for path in (
            "Assets/Resources/Prefab/PlayerPrefab/MaleModel.prefab",
            "Assets/Resources/Prefab/PlayerPrefab/FemaleModel.prefab",
            "Assets/Resources/Prefab/Item/Equip/Weapon/Sword/knife.prefab")]
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2)+"\n")
    except (OSError, ValueError, KeyError) as exc:
        parser.error(str(exc))
    print(f"Inventoried {len(report['states'])} states and {len(report['clips'])} clips: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
