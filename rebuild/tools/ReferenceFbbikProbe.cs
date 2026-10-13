// Launch-only observer of the live player's FullBodyBipedIK solver. It adds two
// read-only delegate subscribers (OnPreRead, OnPostUpdate) and removes them at
// the end; it writes no solver field, transform, input, skill or save.
using System;
using System.IO;
using System.Text;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using UnityEngine;
using Pathea;
using RootMotion.FinalIK;
namespace PeReference {
public sealed class PoseProbe : MonoBehaviour {
    static PoseProbe instance;
    static readonly CultureInfo C = CultureInfo.InvariantCulture;
    const long MaxBytes = 96L * 1024 * 1024;
    const int MaxBones = 32;
    StreamWriter output;
    string arm, stop;
    int samples, frames, maxFrames;
    long bytes;
    float installed, started = -1, seconds, waitSeconds, reported = -10;
    bool finished, pending;
    FullBodyBipedIK ik;
    IKSolverFullBodyBiped solver;
    readonly List<Transform> bones = new List<Transform>();
    readonly StringBuilder pre = new StringBuilder(8192);
    IKSolver.UpdateDelegate onPre, onPost;
    // ChildConstraint keeps its serialized bones private.
    static Transform Private(object value, string field) {
        FieldInfo f = value.GetType().GetField(field, BindingFlags.NonPublic|BindingFlags.Instance);
        if (f == null) throw new InvalidOperationException("Required installed field missing: " + field);
        return f.GetValue(value) as Transform;
    }
    static int Setting(string name, int fallback, int lo, int hi) {
        string s = Environment.GetEnvironmentVariable(name); int n;
        if (String.IsNullOrEmpty(s)) return fallback;
        if (!Int32.TryParse(s, out n) || n < lo || n > hi) throw new ArgumentException(name);
        return n;
    }
    public static void Install() {
        if (instance != null) return;
        GameObject host = new GameObject("ReferenceFbbikProbe (observation only)");
        host.hideFlags = HideFlags.HideAndDontSave; DontDestroyOnLoad(host);
        instance = host.AddComponent<PoseProbe>();
        try { instance.Configure(); } catch { Destroy(host); throw; }
    }
    void Configure() {
        string path = Environment.GetEnvironmentVariable("PE_POSE_OUTPUT");
        arm = Environment.GetEnvironmentVariable("PE_POSE_ARM"); stop = Environment.GetEnvironmentVariable("PE_POSE_STOP");
        if (String.IsNullOrEmpty(path) || !Path.IsPathRooted(path) || String.IsNullOrEmpty(arm) || String.IsNullOrEmpty(stop))
            throw new ArgumentException("Fresh absolute output and arm/stop required");
        maxFrames = Setting("PE_POSE_FRAMES", 1800, 1, 3600);
        seconds = Setting("PE_POSE_SECONDS", 120, 1, 120); waitSeconds = Setting("PE_POSE_WAIT", 900, 1, 1800);
        output = new StreamWriter(new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read), new UTF8Encoding(false));
        installed = Time.realtimeSinceStartup;
        Write("{\"type\":\"header\",\"schema\":1,\"kind\":\"observed-original-fbbik\",\"prePhase\":\"IKSolverFullBody.OnPreRead\",\"postPhase\":\"IKSolver.OnPostUpdate\",\"unity\":" + Q(Application.unityVersion)
            + ",\"probeSourceSha256\":" + Q(Environment.GetEnvironmentVariable("PE_FBBIK_PROBE_SHA256")) + ",\"maxFrames\":" + maxFrames + ",\"maxSeconds\":" + F(seconds) + ",\"maxBytes\":" + MaxBytes + "}");
        output.Flush(); Console.Error.WriteLine("[fbbik] Waiting for arm: " + arm); StartCoroutine(Watch());
    }
    IEnumerator Watch() {
        while (!finished) { yield return new WaitForEndOfFrame(); try { Tick(); } catch (Exception e) { Console.Error.WriteLine("[fbbik] " + e); Finish("error"); } }
        Destroy(gameObject);
    }
    void Tick() {
        float now = Time.realtimeSinceStartup;
        if (File.Exists(stop)) { Finish("stop-file"); return; }
        if (started < 0 && now - installed >= waitSeconds) { Finish("wait-limit"); return; }
        if (!File.Exists(arm)) return;
        if (started < 0) started = now;
        if (now - started >= seconds) { Finish("time-limit"); return; }
        if (samples >= maxFrames || bytes >= MaxBytes - 262144) { Finish("capture-limit"); return; }
        frames++;
        if (solver != null && ik == null) { Finish("solver-destroyed"); return; }
        if (solver == null) Attach();
    }
    void Attach() {
        MainPlayerCmpt main = MainPlayerCmpt.gMainPlayer;
        PeEntity e = main == null ? null : main.GetComponent<PeEntity>();
        BiologyViewCmpt view = e == null || !e.hasView ? null : e.GetComponent<BiologyViewCmpt>();
        // The solver sits on the view root's IK child, a sibling of the model controller's object.
        BiologyViewRoot viewRoot = view == null ? null : view.biologyViewRoot;
        FullBodyBipedIK found = viewRoot == null ? null : viewRoot.fbbik;
        if (found == null && e != null) found = e.GetComponentInChildren<FullBodyBipedIK>();
        if (found == null || !found.isActiveAndEnabled || !found.solver.initiated) {
            if (Time.realtimeSinceStartup - reported >= 2) {
                reported = Time.realtimeSinceStartup;
                Console.Error.WriteLine("[fbbik] Not attached: player=" + (main != null) + " view=" + (view != null) + " viewRoot=" + (viewRoot != null)
                    + " solver=" + (found != null) + (found == null ? "" : " active=" + found.isActiveAndEnabled + " initiated=" + found.solver.initiated));
            }
            return;
        }
        IKSolverFullBodyBiped s = found.solver; Transform root = s.GetRoot();
        List<Transform> used = new List<Transform>();
        foreach (FBIKChain chain in s.chain) foreach (IKSolver.Node node in chain.nodes) used.Add(node.transform);
        foreach (Transform t in s.spineMapping.spineBones) used.Add(t);
        foreach (IKMappingLimb limb in s.limbMappings) if (limb.parentBone != null) used.Add(limb.parentBone);
        foreach (IKMappingBone map in s.boneMappings) used.Add(map.bone);
        used.Add(root);
        Transform top = root.parent != null ? root.parent : root;
        List<Transform> closure = new List<Transform>();
        foreach (Transform t in used) { Transform x = t; while (x != null) { if (!closure.Contains(x)) closure.Add(x); if (x == top) break; x = x.parent; } if (x == null) throw new InvalidOperationException("Solver bone outside the model root"); }
        if (closure.Count > MaxBones) throw new InvalidOperationException("Bone bound");
        List<Transform> order = new List<Transform>(closure);
        order.Sort(delegate(Transform left, Transform right) { int d = Depth(left, top).CompareTo(Depth(right, top)); return d != 0 ? d : String.CompareOrdinal(left.name, right.name); });
        bones.Clear(); bones.AddRange(order);
        StringBuilder b = new StringBuilder(8192);
        b.Append("{\"type\":\"rig\",\"frame\":").Append(Time.frameCount).Append(",\"component\":").Append(Q(found.GetType().FullName))
         .Append(",\"timeStep\":").Append(F(found.timeStep)).Append(",\"fixTransforms\":").Append(B(found.fixTransforms))
         .Append(",\"iterations\":").Append(s.iterations).Append(",\"spineStiffness\":").Append(F(s.spineStiffness))
         .Append(",\"pullBodyVertical\":").Append(F(s.pullBodyVertical)).Append(",\"pullBodyHorizontal\":").Append(F(s.pullBodyHorizontal))
         .Append(",\"root\":").Append(Index(root)).Append(",\"rootNode\":").Append(Index(s.rootNode)).Append(",\"bones\":[");
        for (int i = 0; i < bones.Count; i++) {
            if (i > 0) b.Append(',');
            b.Append("{\"name\":").Append(Q(bones[i].name)).Append(",\"parent\":").Append(bones[i] == top ? -1 : Index(bones[i].parent)).Append(",\"lossyScale\":").Append(V(bones[i].lossyScale)).Append('}');
        }
        b.Append("],\"chains\":[");
        for (int c = 0; c < s.chain.Length; c++) {
            FBIKChain chain = s.chain[c]; IKConstraintBend bend = chain.bendConstraint;
            if (c > 0) b.Append(',');
            b.Append("{\"nodes\":["); for (int n = 0; n < chain.nodes.Length; n++) { if (n > 0) b.Append(','); b.Append(Index(chain.nodes[n].transform)); }
            b.Append("],\"pin\":").Append(F(chain.pin)).Append(",\"pull\":").Append(F(chain.pull)).Append(",\"push\":").Append(F(chain.push)).Append(",\"pushParent\":").Append(F(chain.pushParent))
             .Append(",\"reach\":").Append(F(chain.reach)).Append(",\"reachSmoothing\":").Append((int)chain.reachSmoothing).Append(",\"pushSmoothing\":").Append((int)chain.pushSmoothing)
             .Append(",\"bendInitiated\":").Append(B(bend.initiated)).Append(",\"bendWeight\":").Append(F(bend.weight)).Append(",\"bendGoal\":").Append(B(bend.bendGoal != null))
             .Append(",\"bendDirection\":").Append(V(bend.direction)).Append(",\"bendDefaultLocal\":").Append(V(bend.defaultLocalDirection)).Append(",\"bendDefaultChild\":").Append(V(bend.defaultChildDirection))
             .Append(",\"constraints\":[");
            for (int k = 0; k < chain.childConstraints.Length; k++) {
                FBIKChain.ChildConstraint cc = chain.childConstraints[k]; if (k > 0) b.Append(',');
                b.Append("{\"bone1\":").Append(Index(Private(cc, "bone1"))).Append(",\"bone2\":").Append(Index(Private(cc, "bone2"))).Append(",\"pushElasticity\":").Append(F(cc.pushElasticity)).Append(",\"pullElasticity\":").Append(F(cc.pullElasticity)).Append('}');
            }
            b.Append("]}");
        }
        b.Append("],\"effectorBones\":["); for (int i = 0; i < s.effectors.Length; i++) { if (i > 0) b.Append(','); b.Append(Index(s.effectors[i].bone)); }
        b.Append("],\"spine\":["); for (int i = 0; i < s.spineMapping.spineBones.Length; i++) { if (i > 0) b.Append(','); b.Append(Index(s.spineMapping.spineBones[i])); }
        b.Append("],\"spineIterations\":").Append(s.spineMapping.iterations).Append(",\"spineTwistWeight\":").Append(F(s.spineMapping.twistWeight)).Append(",\"limbs\":[");
        for (int i = 0; i < s.limbMappings.Length; i++) {
            IKMappingLimb limb = s.limbMappings[i]; if (i > 0) b.Append(',');
            b.Append("{\"parent\":").Append(limb.parentBone == null ? -1 : Index(limb.parentBone)).Append(",\"maintainRotationWeight\":").Append(F(limb.maintainRotationWeight)).Append(",\"weight\":").Append(F(limb.weight)).Append('}');
        }
        b.Append("],\"head\":").Append(Index(s.boneMappings[0].bone)).Append(",\"headMaintainRotationWeight\":").Append(F(s.boneMappings[0].maintainRotationWeight));
        // Subscription order of the offset writers; names only.
        b.Append(",\"preUpdateSubscribers\":["); Delegate[] list = s.OnPreUpdate == null ? new Delegate[0] : s.OnPreUpdate.GetInvocationList();
        for (int i = 0; i < list.Length && i < 16; i++) { if (i > 0) b.Append(','); b.Append(Q((list[i].Target == null ? "static" : list[i].Target.GetType().FullName) + "::" + list[i].Method.Name)); }
        b.Append("],\"modelBehaviours\":["); MonoBehaviour[] all = found.GetComponents<MonoBehaviour>();
        for (int i = 0; i < all.Length && i < 64; i++) { if (i > 0) b.Append(','); b.Append("{\"type\":").Append(Q(all[i] == null ? null : all[i].GetType().FullName)).Append(",\"enabled\":").Append(B(all[i] != null && all[i].enabled)).Append('}'); }
        b.Append("]}"); Write(b.ToString()); output.Flush();
        ik = found; solver = s; onPre = Pre; onPost = Post;
        solver.OnPreRead = (IKSolver.UpdateDelegate)Delegate.Combine(solver.OnPreRead, onPre);
        solver.OnPostUpdate = (IKSolver.UpdateDelegate)Delegate.Combine(solver.OnPostUpdate, onPost);
        Console.Error.WriteLine("[fbbik] Attached to solver with " + bones.Count + " bones");
    }
    static int Depth(Transform t, Transform top) { int d = 0; while (t != top) { t = t.parent; d++; } return d; }
    int Index(Transform t) { int i = bones.IndexOf(t); if (i < 0) throw new InvalidOperationException("Unlisted solver transform"); return i; }
    void Pose(StringBuilder b) {
        b.Append('[');
        for (int i = 0; i < bones.Count; i++) {
            Transform t = bones[i]; if (i > 0) b.Append(',');
            // The top bone is recorded in world space; the rest are local.
            b.Append('[').Append(V(i == 0 ? t.position : t.localPosition)).Append(',').Append(R(i == 0 ? t.rotation : t.localRotation)).Append(']');
        }
        b.Append(']');
    }
    // After every OnPreUpdate offset writer and the weight clamp, before ReadPose.
    void Pre() {
        if (finished || solver == null) return;
        try {
            pre.Length = 0;
            pre.Append("\"weight\":").Append(F(solver.IKPositionWeight)).Append(",\"effectors\":[");
            for (int i = 0; i < solver.effectors.Length; i++) {
                IKEffector e = solver.effectors[i]; if (i > 0) pre.Append(',');
                pre.Append("{\"positionOffset\":").Append(V(e.positionOffset)).Append(",\"position\":").Append(V(e.position)).Append(",\"rotation\":").Append(R(e.rotation))
                   .Append(",\"positionWeight\":").Append(F(e.positionWeight)).Append(",\"rotationWeight\":").Append(F(e.rotationWeight))
                   .Append(",\"maintain\":").Append(F(e.maintainRelativePositionWeight)).Append(",\"target\":").Append(B(e.target != null)).Append('}');
            }
            pre.Append("],\"bendWeights\":[");
            for (int c = 1; c < solver.chain.Length; c++) { if (c > 1) pre.Append(','); pre.Append(F(solver.chain[c].bendConstraint.weight)); }
            pre.Append("],\"pre\":"); Pose(pre); pending = true;
        } catch (Exception e) { Console.Error.WriteLine("[fbbik] " + e); Finish("error"); }
    }
    void Post() {
        if (finished || solver == null || !pending) return;
        pending = false;
        try {
            if (!File.Exists(arm) || samples >= maxFrames || bytes >= MaxBytes - 262144) return;
            StringBuilder b = new StringBuilder(16384);
            b.Append("{\"type\":\"frame\",\"sample\":").Append(samples).Append(",\"frame\":").Append(Time.frameCount).Append(",\"time\":").Append(F(Time.time)).Append(",\"dt\":").Append(F(Time.deltaTime)).Append(',')
             .Append(pre).Append(",\"post\":"); Pose(b);
            b.Append(",\"nodes\":[");
            for (int c = 0; c < solver.chain.Length; c++) for (int n = 0; n < solver.chain[c].nodes.Length; n++) { if (c + n > 0) b.Append(','); b.Append(V(solver.chain[c].nodes[n].solverPosition)); }
            b.Append("]}"); Write(b.ToString()); samples++;
            if ((samples & 31) == 0) output.Flush();
        } catch (Exception e) { Console.Error.WriteLine("[fbbik] " + e); Finish("error"); }
    }
    static string B(bool v) { return v ? "true" : "false"; }
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
    void Write(string line) { int n = Encoding.UTF8.GetByteCount(line) + 1; if (n > 262144 || bytes + n > MaxBytes) throw new InvalidOperationException("Capture size bound"); output.WriteLine(line); bytes += n; }
    void Finish(string reason) {
        if (finished) return; finished = true;
        if (solver != null && ik != null) {
            solver.OnPreRead = (IKSolver.UpdateDelegate)Delegate.Remove(solver.OnPreRead, onPre);
            solver.OnPostUpdate = (IKSolver.UpdateDelegate)Delegate.Remove(solver.OnPostUpdate, onPost);
        }
        if (output != null) { Write("{\"type\":\"end\",\"reason\":" + Q(reason) + ",\"frames\":" + samples + ",\"observedLoops\":" + frames + "}"); output.Dispose(); output = null; }
        Console.Error.WriteLine("[fbbik] Finished: " + reason + " samples=" + samples);
    }
    void OnDestroy() { Finish("destroy"); }
}}
