// Observation-only: existing mission agents, no spawn/RNG/input/save calls.
using System;
using System.IO;
using System.Text;
using System.Collections;
using System.Globalization;
using System.Reflection;
using UnityEngine;
using Pathea;
namespace PeReference {
public sealed class PoseProbe : MonoBehaviour {
    static PoseProbe instance;
    StreamWriter output;
    string arm, stop;
    int frames, maxFrames;
    long bytes;
    float installed, started = -1, waitSeconds, seconds;
    bool finished;
    FieldInfo agentsField, beaconField, waveField, prepField, centerField, forwardField, dataField, uiField, entityManagerField;
    const long MaxBytes = 16L * 1024 * 1024;
    static readonly CultureInfo C = CultureInfo.InvariantCulture;
    public static void Install() {
        if (instance != null) return;
        GameObject host = new GameObject("ReferenceTownDefenseProbe (observation only)");
        host.hideFlags = HideFlags.HideAndDontSave;
        DontDestroyOnLoad(host);
        instance = host.AddComponent<PoseProbe>();
        try { instance.Configure(); }
        catch { Destroy(host); throw; }
    }
    static int Setting(string name, int fallback, int min, int max) {
        string value = Environment.GetEnvironmentVariable(name); int n;
        if (String.IsNullOrEmpty(value)) return fallback;
        if (!Int32.TryParse(value, out n) || n < min || n > max) throw new ArgumentException(name);
        return n;
    }
    void Configure() {
        string path = Environment.GetEnvironmentVariable("PE_POSE_OUTPUT");
        arm = Environment.GetEnvironmentVariable("PE_POSE_ARM");
        stop = Environment.GetEnvironmentVariable("PE_POSE_STOP");
        if (String.IsNullOrEmpty(path) || !Path.IsPathRooted(path) || String.IsNullOrEmpty(arm) || String.IsNullOrEmpty(stop))
            throw new ArgumentException("Fresh absolute output and arm/stop paths required");
        maxFrames = Setting("PE_POSE_FRAMES", 3600, 1, 3600);
        seconds = Setting("PE_POSE_SECONDS", 120, 1, 120);
        waitSeconds = Setting("PE_POSE_WAIT", 900, 1, 1800);
        beaconField = Field(typeof(SceneEntityMissionPointTowerDefence), "_entityBcn", typeof(EntityMonsterBeacon));
        dataField = Field(typeof(SceneEntityMissionPointTowerDefence), "_towerData", typeof(TypeTowerDefendsData));
        agentsField = Field(typeof(EntityMonsterBeacon), "_agents", typeof(System.Collections.Generic.List<ISceneObjAgent>));
        waveField = Field(typeof(EntityMonsterBeacon), "_idxWave", typeof(int));
        prepField = Field(typeof(EntityMonsterBeacon), "_preTime", typeof(float));
        centerField = Field(typeof(EntityMonsterBeacon), "_position", typeof(Vector3));
        forwardField = Field(typeof(EntityMonsterBeacon), "_forward", typeof(Vector3));
        uiField = Field(typeof(EntityMonsterBeacon), "_uiData", typeof(TowerInfoUIData));
        // EntityMgr.Instance lazily constructs gameplay state. Read its existing backing field instead.
        entityManagerField = typeof(PeSingleton<EntityMgr>).GetField("instance", BindingFlags.NonPublic | BindingFlags.Static);
        if (entityManagerField == null || entityManagerField.FieldType != typeof(EntityMgr))
            throw new InvalidOperationException("Existing EntityMgr backing field unavailable");
        output = new StreamWriter(new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read), new UTF8Encoding(false));
        installed = Time.realtimeSinceStartup;
        Write("{\"type\":\"header\",\"schema\":1,\"kind\":\"observed-original-town-defense\",\"mission\":9077,\"target\":7507,\"phase\":\"WaitForEndOfFrame\",\"unity\":" + Q(Application.unityVersion) +
              ",\"probeSourceSha256\":" + Q(Environment.GetEnvironmentVariable("PE_TOWN_DEFENSE_PROBE_SHA256")) + ",\"maxFrames\":" + maxFrames + ",\"maxBytes\":" + MaxBytes + "}");
        output.Flush();
        Console.Error.WriteLine("[defense] Waiting for arm file: " + arm);
        StartCoroutine(Capture());
    }
    IEnumerator Capture() {
        while (!finished) {
            yield return new WaitForEndOfFrame();
            try { Tick(); }
            catch (Exception error) { Console.Error.WriteLine("[defense] Failed: " + error); Finish("error"); }
        }
        Destroy(gameObject);
    }
    void Tick() {
        float now = Time.realtimeSinceStartup;
        if (File.Exists(stop)) { Finish("stop-file"); return; }
        if (started < 0 && now - installed >= waitSeconds) { Finish("wait-limit"); return; }
        if (!File.Exists(arm)) return;
        if (started < 0) started = now;
        if (now - started >= seconds) { Finish("time-limit"); return; }
        if (frames >= maxFrames || bytes >= MaxBytes - 66560) { Finish("capture-limit"); return; }
        // Existing fields/read-only lookup only: no lazy singleton construction.
        MainPlayerCmpt main = MainPlayerCmpt.gMainPlayer;
        PeEntity player = main == null ? null : main.GetComponent<PeEntity>();
        SceneEntityCreator creator = SceneEntityCreator.self;
        SceneEntityMissionPointTowerDefence point = creator == null ? null : creator.GetMissionPoint(9077, 7507) as SceneEntityMissionPointTowerDefence;
        EntityMonsterBeacon beacon = point == null ? null : beaconField.GetValue(point) as EntityMonsterBeacon;
        TypeTowerDefendsData data = point == null ? null : dataField.GetValue(point) as TypeTowerDefendsData;
        IList agents = beacon == null ? null : agentsField.GetValue(beacon) as IList;
        if (beacon != null && agents == null) throw new InvalidOperationException("Unexpected beacon list type");
        if (agents != null && agents.Count > 64) throw new InvalidOperationException("Beacon list exceeds observation bound");
        if (data != null && (data.m_iNpcList == null || data.m_iNpcList.Count > 64))
            throw new InvalidOperationException("Protected NPC list exceeds observation bound");
        MissionManager manager = MissionManager.Instance;
        PlayerMission missions = manager == null ? null : manager.m_PlayerMission;
        bool active = missions != null && missions.HasMission(9077);
        string counter = active ? missions.GetQuestVariable(9077, "TDMONS0") : null;
        StringBuilder b = new StringBuilder(16384);
        b.Append("{\"type\":\"frame\",\"sample\":").Append(frames).Append(",\"frame\":").Append(Time.frameCount)
         .Append(",\"time\":").Append(F(Time.time)).Append(",\"realtime\":").Append(F(now))
         .Append(",\"missionActive\":").Append(active ? "true" : "false")
         .Append(",\"counter\":").Append(Q(counter)).Append(",\"missionPointPresent\":").Append(point == null ? "false" : "true")
         .Append(",\"player\":").Append(Entity(player)).Append(",\"beacon\":");
        if (beacon == null) b.Append("null");
        else {
            TowerInfoUIData ui = uiField.GetValue(beacon) as TowerInfoUIData;
            b.Append("{\"id\":").Append(beacon.Id).Append(",\"waveIndex\":").Append((int)waveField.GetValue(beacon))
             .Append(",\"prepTime\":").Append(F((float)prepField.GetValue(beacon)))
             .Append(",\"position\":").Append(V((Vector3)centerField.GetValue(beacon)))
             .Append(",\"forward\":").Append(V((Vector3)forwardField.GetValue(beacon)))
             .Append(",\"ui\":");
            if (ui == null) b.Append("null");
            else b.Append("{\"mission\":").Append(ui.MissionID).Append(",\"count\":").Append(ui.CurCount)
                  .Append(",\"goal\":").Append(ui.MaxCount).Append(",\"remainingWaves\":").Append(ui.CurWavesRemaining)
                  .Append(",\"totalWaves\":").Append(ui.TotalWaves).Append('}');
            b.Append('}');
        }
        b.Append(",\"protectedNpcs\":[");
        EntityMgr entities = entityManagerField.GetValue(null) as EntityMgr;
        if (data != null) for (int i = 0; i < data.m_iNpcList.Count; i++) {
            if (i != 0) b.Append(',');
            int id = data.m_iNpcList[i]; PeEntity npc = null;
            if (entities != null) entities.mDicEntity.TryGetValue(id, out npc);
            b.Append("{\"id\":").Append(id).Append(",\"entity\":").Append(Entity(npc)).Append('}');
        }
        b.Append("],\"agents\":[");
        if (agents != null) for (int i = 0; i < agents.Count; i++) {
            SceneEntityPosAgent a = agents[i] as SceneEntityPosAgent;
            if (a == null) throw new InvalidOperationException("Unexpected mission agent class");
            if (i != 0) b.Append(',');
            b.Append("{\"sourceOrder\":").Append(i).Append(",\"agentId\":").Append(a.Id)
             .Append(",\"prototype\":").Append(a.protoId).Append(",\"step\":").Append(Q(a.Step.ToString()))
             .Append(",\"agentPosition\":").Append(V(a.Pos)).Append(",\"agentScale\":").Append(V(a.Scl))
             .Append(",\"agentRotation\":").Append(R(a.Rot)).Append(",\"canRide\":").Append(a.canRide ? "true" : "false")
             .Append(",\"entity\":").Append(Entity(a.entity)).Append('}');
        }
        b.Append("]}");
        if (Encoding.UTF8.GetByteCount(b.ToString()) > 65536) throw new InvalidOperationException("Frame exceeds64KiB");
        Write(b.ToString()); output.Flush(); frames++;
    }
    static FieldInfo Field(Type type, string name, Type expected) {
        FieldInfo f = type.GetField(name, BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.Instance);
        if (f == null || f.FieldType != expected) throw new InvalidOperationException("Incompatible source field " + type.Name + "." + name);
        return f;
    }
    static string Entity(PeEntity e) {
        if (e == null) return "null";
        PeTrans t = e.peTrans;
        return "{\"id\":" + e.Id + ",\"prototype\":" + e.ProtoID + ",\"prototypeType\":" + (int)e.proto +
            ",\"position\":" + V(e.position) + ",\"rotation\":" + R(e.rotation) +
            ",\"scale\":" + (t == null ? "null" : V(t.scale)) +
            ",\"hp\":" + F(e.GetAttribute(AttribType.Hp)) + ",\"maxHp\":" + F(e.GetAttribute(AttribType.HpMax)) +
            ",\"attack\":" + F(e.GetAttribute(AttribType.Atk)) + ",\"defense\":" + F(e.GetAttribute(AttribType.Def)) +
            ",\"campId\":" + F(e.GetAttribute(AttribType.CampID)) + ",\"damageId\":" + F(e.GetAttribute(AttribType.DamageID)) +
            ",\"defaultPlayerId\":" + F(e.GetAttribute(AttribType.DefaultPlayerID)) + "}";
    }
    static string F(float n) { return Single.IsNaN(n) || Single.IsInfinity(n) ? "null" : n.ToString("R", C); }
    static string V(Vector3 v) { return "[" + F(v.x) + "," + F(v.y) + "," + F(v.z) + "]"; }
    static string R(Quaternion q) { return "[" + F(q.x) + "," + F(q.y) + "," + F(q.z) + "," + F(q.w) + "]"; }
    static string Q(string s) {
        if (s == null) return "null";
        StringBuilder b = new StringBuilder("\"");
        foreach (char c in s) {
            if (c == '\\' || c == '"') b.Append('\\').Append(c);
            else if (c < 32) b.Append("\\u").Append(((int)c).ToString("x4", C));
            else b.Append(c);
        }
        return b.Append('"').ToString();
    }
    void Write(string line) { output.WriteLine(line); bytes += Encoding.UTF8.GetByteCount(line) + 1; }
    void Finish(string reason) {
        if (finished) return; finished = true;
        if (output != null) { Write("{\"type\":\"end\",\"reason\":" + Q(reason) + ",\"frames\":" + frames + "}"); output.Dispose(); output = null; }
        Console.Error.WriteLine("[defense] Finished: " + reason);
    }
    void OnDestroy() { Finish("destroy"); }
}
}
