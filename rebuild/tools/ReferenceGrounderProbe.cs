// Launch-only observer of the live player's GrounderFBBIK. It adds two read-only
// subscribers to the grounder's own OnPreGrounder/OnPostGrounder delegates and
// removes them at the end. Before each grounder update it repeats the grounder's
// seven rays as plain Physics.Raycast queries. It writes no grounder or solver
// field, transform, input, skill or save.
using System;
using System.IO;
using System.Text;
using System.Collections;
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
    StreamWriter output;
    string arm, stop;
    int samples, frames, maxFrames;
    long bytes;
    float installed, started = -1, seconds, waitSeconds, reported = -10;
    bool finished, pending;
    FullBodyBipedIK ik;
    GrounderFBBIK grounder;
    Grounding grounding;
    readonly StringBuilder pre = new StringBuilder(8192);
    Grounder.GrounderDelegate onPre, onPost;
    // The compiled classes keep their per-frame state private.
    static object Private(object value, string field) {
        FieldInfo f = value.GetType().GetField(field, BindingFlags.NonPublic|BindingFlags.Public|BindingFlags.Instance);
        if (f == null) throw new InvalidOperationException("Required installed field missing: " + field);
        return f.GetValue(value);
    }
    static int Setting(string name, int fallback, int lo, int hi) {
        string s = Environment.GetEnvironmentVariable(name); int n;
        if (String.IsNullOrEmpty(s)) return fallback;
        if (!Int32.TryParse(s, out n) || n < lo || n > hi) throw new ArgumentException(name);
        return n;
    }
    public static void Install() {
        if (instance != null) return;
        GameObject host = new GameObject("ReferenceGrounderProbe (observation only)");
        host.hideFlags = HideFlags.HideAndDontSave; DontDestroyOnLoad(host);
        instance = host.AddComponent<PoseProbe>();
        try { instance.Configure(); } catch { Destroy(host); throw; }
    }
    void Configure() {
        string path = Environment.GetEnvironmentVariable("PE_POSE_OUTPUT");
        arm = Environment.GetEnvironmentVariable("PE_POSE_ARM"); stop = Environment.GetEnvironmentVariable("PE_POSE_STOP");
        if (String.IsNullOrEmpty(path) || !Path.IsPathRooted(path) || String.IsNullOrEmpty(arm) || String.IsNullOrEmpty(stop))
            throw new ArgumentException("Fresh absolute output and arm/stop required");
        maxFrames = Setting("PE_POSE_FRAMES", 3600, 1, 3600);
        seconds = Setting("PE_POSE_SECONDS", 120, 1, 120); waitSeconds = Setting("PE_POSE_WAIT", 900, 1, 1800);
        output = new StreamWriter(new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read), new UTF8Encoding(false));
        installed = Time.realtimeSinceStartup;
        Write("{\"type\":\"header\",\"schema\":1,\"kind\":\"observed-original-grounder\",\"prePhase\":\"Grounder.OnPreGrounder\",\"postPhase\":\"Grounder.OnPostGrounder\",\"unity\":" + Q(Application.unityVersion)
            + ",\"probeSourceSha256\":" + Q(Environment.GetEnvironmentVariable("PE_PROBE_SHA256")) + ",\"maxFrames\":" + maxFrames + ",\"maxSeconds\":" + F(seconds) + ",\"maxBytes\":" + MaxBytes + "}");
        output.Flush(); Console.Error.WriteLine("[grounder] Waiting for arm: " + arm); StartCoroutine(Watch());
    }
    IEnumerator Watch() {
        while (!finished) { yield return new WaitForEndOfFrame(); try { Tick(); } catch (Exception e) { Console.Error.WriteLine("[grounder] " + e); Finish("error"); } }
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
        if (grounding != null && grounder == null) { Finish("grounder-destroyed"); return; }
        if (grounding == null) Attach();
    }
    HumanPhyCtrl phy;
    void Attach() {
        MainPlayerCmpt main = MainPlayerCmpt.gMainPlayer;
        PeEntity e = main == null ? null : main.GetComponent<PeEntity>();
        BiologyViewCmpt view = e == null || !e.hasView ? null : e.GetComponent<BiologyViewCmpt>();
        BiologyViewRoot viewRoot = view == null ? null : view.biologyViewRoot;
        FullBodyBipedIK body = viewRoot == null ? null : viewRoot.fbbik;
        if (body == null && e != null) body = e.GetComponentInChildren<FullBodyBipedIK>();
        GrounderFBBIK found = body == null ? null : body.GetComponent<GrounderFBBIK>();
        if (found == null && e != null) found = e.GetComponentInChildren<GrounderFBBIK>();
        bool ready = found != null && found.solver != null && found.solver.legs != null && found.solver.legs.Length == 2 && found.solver.pelvis != null && found.solver.root != null;
        if (!ready || body == null || found.ik != body || !body.solver.initiated) {
            if (Time.realtimeSinceStartup - reported >= 2) {
                reported = Time.realtimeSinceStartup;
                Console.Error.WriteLine("[grounder] Not attached: player=" + (main != null) + " view=" + (view != null) + " solver=" + (body != null) + " grounder=" + (found != null) + " ready=" + ready);
            }
            return;
        }
        Grounding g = found.solver; Transform[] feet = (Transform[])Private(found, "feet");
        StringBuilder b = new StringBuilder(4096);
        b.Append("{\"type\":\"rig\",\"frame\":").Append(Time.frameCount).Append(",\"component\":").Append(Q(found.GetType().FullName))
         .Append(",\"enabled\":").Append(B(found.isActiveAndEnabled)).Append(",\"weight\":").Append(F(found.weight))
         .Append(",\"spineBend\":").Append(F(found.spineBend)).Append(",\"spineSpeed\":").Append(F(found.spineSpeed)).Append(",\"spine\":[");
        for (int i = 0; i < found.spine.Length; i++) {
            if (i > 0) b.Append(',');
            b.Append("{\"effector\":").Append((int)found.spine[i].effectorType).Append(",\"horizontal\":").Append(F(found.spine[i].horizontalWeight)).Append(",\"vertical\":").Append(F(found.spine[i].verticalWeight)).Append('}');
        }
        b.Append("],\"layers\":").Append(g.layers.value).Append(",\"layerNames\":[");
        for (int i = 0; i < 32; i++) { if (i > 0) b.Append(','); b.Append(Q(LayerMask.LayerToName(i))); }
        b.Append("],\"maxStep\":").Append(F(g.maxStep)).Append(",\"heightOffset\":").Append(F(g.heightOffset)).Append(",\"footSpeed\":").Append(F(g.footSpeed))
         .Append(",\"footRadius\":").Append(F(g.footRadius)).Append(",\"prediction\":").Append(F(g.prediction)).Append(",\"footRotationWeight\":").Append(F(g.footRotationWeight))
         .Append(",\"footRotationSpeed\":").Append(F(g.footRotationSpeed)).Append(",\"maxFootRotationAngle\":").Append(F(g.maxFootRotationAngle)).Append(",\"rotateSolver\":").Append(B(g.rotateSolver))
         .Append(",\"pelvisSpeed\":").Append(F(g.pelvisSpeed)).Append(",\"pelvisDamper\":").Append(F(g.pelvisDamper)).Append(",\"lowerPelvisWeight\":").Append(F(g.lowerPelvisWeight))
         .Append(",\"liftPelvisWeight\":").Append(F(g.liftPelvisWeight)).Append(",\"rootSphereCastRadius\":").Append(F(g.rootSphereCastRadius)).Append(",\"quality\":").Append((int)g.quality)
         .Append(",\"root\":").Append(Q(g.root.name)).Append(",\"pelvis\":").Append(Q(body.references.pelvis.name)).Append(",\"feet\":[");
        for (int i = 0; i < feet.Length; i++) { if (i > 0) b.Append(','); b.Append(Q(feet[i].name)); }
        b.Append("],\"legTransforms\":["); for (int i = 0; i < g.legs.Length; i++) { if (i > 0) b.Append(','); b.Append(Q(g.legs[i].transform.name)); }
        b.Append("],\"footEffectors\":["); for (int i = 0; i < 2; i++) { if (i > 0) b.Append(','); b.Append(Q(body.solver.GetEffector(i == 0 ? FullBodyBipedEffector.LeftFoot : FullBodyBipedEffector.RightFoot).bone.name)); }
        b.Append("],\"rootLossyScale\":").Append(V(g.root.lossyScale));
        // Names only: what else listens before and after the grounder.
        b.Append(",\"preGrounderSubscribers\":").Append(Names(found.OnPreGrounder)).Append(",\"postGrounderSubscribers\":").Append(Names(found.OnPostGrounder))
         .Append(",\"preUpdateSubscribers\":").Append(Names(body.solver.OnPreUpdate)).Append('}');
        Write(b.ToString()); output.Flush();
        ik = body; grounder = found; grounding = g; onPre = Pre; onPost = Post; phy = viewRoot == null ? null : viewRoot.humanPhyCtrl;
        grounder.OnPreGrounder = (Grounder.GrounderDelegate)Delegate.Combine(grounder.OnPreGrounder, onPre);
        grounder.OnPostGrounder = (Grounder.GrounderDelegate)Delegate.Combine(grounder.OnPostGrounder, onPost);
        Console.Error.WriteLine("[grounder] Attached; layers=" + g.layers.value);
    }
    static string Names(Delegate d) {
        StringBuilder b = new StringBuilder("["); Delegate[] list = d == null ? new Delegate[0] : d.GetInvocationList();
        for (int i = 0; i < list.Length && i < 16; i++) { if (i > 0) b.Append(','); b.Append(Q((list[i].Target == null ? "static" : list[i].Target.GetType().FullName) + "::" + list[i].Method.Name)); }
        return b.Append(']').ToString();
    }
    void Offsets(StringBuilder b) {
        b.Append('['); IKEffector[] list = ik.solver.effectors;
        for (int i = 0; i < list.Length; i++) { if (i > 0) b.Append(','); b.Append(V(list[i].positionOffset)); }
        b.Append(']');
    }
    // Physics.Raycast as Leg.GetRaycastHit and Grounding.GetRootHit issue it. A query only.
    void Ray(StringBuilder b, Vector3 origin, float distance) {
        RaycastHit hit; bool found = Physics.Raycast(origin, Vector3.down, out hit, distance, grounding.layers);
        b.Append("{\"origin\":").Append(V(origin)).Append(",\"distance\":").Append(F(distance)).Append(",\"hit\":").Append(B(found));
        if (found) b.Append(",\"point\":").Append(V(hit.point)).Append(",\"normal\":").Append(V(hit.normal)).Append(",\"hitDistance\":").Append(F(hit.distance))
            .Append(",\"layer\":").Append(hit.collider.gameObject.layer).Append(",\"collider\":").Append(Q(hit.collider.GetType().Name + ":" + hit.collider.name)).Append(",\"trigger\":").Append(B(hit.collider.isTrigger));
        b.Append('}');
    }
    // Immediately before Grounding.Update: the state it starts from and the rays it is about to cast.
    void Pre() {
        if (finished || grounding == null) return;
        try {
            Transform root = grounding.root; Grounding.Pelvis pelvis = grounding.pelvis; float step = grounding.maxStep;
            pre.Length = 0;
            pre.Append("\"enabled\":").Append(B(grounder.enabled)).Append(",\"weight\":").Append(F(grounder.weight))
               .Append(",\"root\":[").Append(V(root.position)).Append(',').Append(V(root.forward)).Append(',').Append(V(root.right)).Append(',').Append(V(root.up)).Append(']')
               .Append(",\"pelvisBone\":").Append(V(ik.references.pelvis.position)).Append(",\"spineOffset\":").Append(V((Vector3)Private(grounder, "spineOffset")))
               .Append(",\"offsets\":"); Offsets(pre);
            pre.Append(",\"pelvis\":{\"lastTime\":").Append(F((float)Private(pelvis, "lastTime"))).Append(",\"heightOffset\":").Append(F(pelvis.heightOffset))
               .Append(",\"damperF\":").Append(F((float)Private(pelvis, "damperF"))).Append(",\"lastRootPosition\":").Append(V((Vector3)Private(pelvis, "lastRootPosition"))).Append('}');
            pre.Append(",\"legs\":[");
            for (int i = 0; i < 2; i++) {
                Grounding.Leg leg = grounding.legs[i]; if (i > 0) pre.Append(',');
                pre.Append("{\"position\":").Append(V(leg.transform.position)).Append(",\"rotation\":").Append(R(leg.transform.rotation))
                   .Append(",\"lastTime\":").Append(F((float)Private(leg, "lastTime"))).Append(",\"lastPosition\":").Append(V((Vector3)Private(leg, "lastPosition")))
                   .Append(",\"ikOffset\":").Append(F(leg.IKOffset)).Append(",\"r\":").Append(R((Quaternion)Private(leg, "r"))).Append(",\"rotationOffset\":").Append(R(leg.rotationOffset)).Append('}');
            }
            pre.Append("],\"rays\":["); Ray(pre, root.position + Vector3.up * step, step * 11);
            for (int i = 0; i < 2; i++) {
                Grounding.Leg leg = grounding.legs[i]; Vector3 foot = leg.transform.position; float dt = Time.time - (float)Private(leg, "lastTime");
                if (dt == 0) continue;   // Leg.Process returns before casting
                Vector3 velocity = (foot - (Vector3)Private(leg, "lastPosition")) / dt; velocity.y = 0;
                Vector3 lift = Vector3.up * step;
                pre.Append(','); Ray(pre, foot + lift, step * 3);
                pre.Append(','); Ray(pre, foot + (root.forward * grounding.footRadius + velocity * grounding.prediction) + lift, step * 3);
                pre.Append(','); Ray(pre, foot + root.right * grounding.footRadius * 0.5f + lift, step * 3);
            }
            pre.Append(']'); pending = true;
        } catch (Exception e) { Console.Error.WriteLine("[grounder] " + e); Finish("error"); }
    }
    // After the pelvis move, both SetLegIK calls and the spine requests.
    void Post() {
        if (finished || grounding == null || !pending) return;
        pending = false;
        try {
            if (!File.Exists(arm) || samples >= maxFrames || bytes >= MaxBytes - 262144) return;
            StringBuilder b = new StringBuilder(8192); Grounding.Pelvis pelvis = grounding.pelvis; RaycastHit rootHit = grounding.rootHit;
            b.Append("{\"type\":\"frame\",\"sample\":").Append(samples).Append(",\"frame\":").Append(Time.frameCount).Append(",\"time\":").Append(F(Time.time)).Append(",\"dt\":").Append(F(Time.deltaTime))
             // Motion_Move_Human and IKCmpt rewrite these two while playing.
             .Append(",\"spineBend\":").Append(F(grounder.spineBend)).Append(",\"maxFootRotationAngle\":").Append(F(grounding.maxFootRotationAngle))
             // The curve's input, and the arm mapping weight IKCmpt zeroes under the same spine mask.
             .Append(",\"forwardGroundAngle\":").Append(phy == null ? "null" : F(phy.forwardGroundAngle)).Append(",\"armMappingWeight\":").Append(F(ik.solver.leftArmMapping.weight)).Append(',')
             .Append(pre).Append(",\"post\":{\"pelvisBone\":").Append(V(ik.references.pelvis.position)).Append(",\"spineOffset\":").Append(V((Vector3)Private(grounder, "spineOffset")))
             .Append(",\"offsets\":"); Offsets(b);
            b.Append(",\"grounded\":").Append(B(grounding.isGrounded)).Append(",\"rootGrounded\":").Append(B(grounding.rootGrounded))
             .Append(",\"rootHit\":{\"point\":").Append(V(rootHit.point)).Append(",\"distance\":").Append(F(rootHit.distance)).Append('}')
             .Append(",\"pelvis\":{\"ikOffset\":").Append(V(pelvis.IKOffset)).Append(",\"heightOffset\":").Append(F(pelvis.heightOffset)).Append(",\"damperF\":").Append(F((float)Private(pelvis, "damperF"))).Append('}')
             .Append(",\"legs\":[");
            for (int i = 0; i < 2; i++) {
                Grounding.Leg leg = grounding.legs[i]; RaycastHit heel = (RaycastHit)Private(leg, "heelHit"); if (i > 0) b.Append(',');
                b.Append("{\"position\":").Append(V(leg.transform.position)).Append(",\"rotation\":").Append(R(leg.transform.rotation))
                 .Append(",\"ikOffset\":").Append(F(leg.IKOffset)).Append(",\"ikPosition\":").Append(V(leg.IKPosition)).Append(",\"rotationOffset\":").Append(R(leg.rotationOffset))
                 .Append(",\"r\":").Append(R((Quaternion)Private(leg, "r"))).Append(",\"heightFromGround\":").Append(F(leg.heightFromGround)).Append(",\"grounded\":").Append(B(leg.isGrounded))
                 .Append(",\"velocity\":").Append(V(leg.velocity)).Append(",\"heelPoint\":").Append(V(heel.point)).Append(",\"heelNormal\":").Append(V(heel.normal)).Append('}');
            }
            b.Append("]}}"); Write(b.ToString()); samples++;
            if ((samples & 31) == 0) output.Flush();
        } catch (Exception e) { Console.Error.WriteLine("[grounder] " + e); Finish("error"); }
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
        if (grounding != null && grounder != null) {
            grounder.OnPreGrounder = (Grounder.GrounderDelegate)Delegate.Remove(grounder.OnPreGrounder, onPre);
            grounder.OnPostGrounder = (Grounder.GrounderDelegate)Delegate.Remove(grounder.OnPostGrounder, onPost);
        }
        if (output != null) { Write("{\"type\":\"end\",\"reason\":" + Q(reason) + ",\"frames\":" + samples + ",\"observedLoops\":" + frames + "}"); output.Dispose(); output = null; }
        Console.Error.WriteLine("[grounder] Finished: " + reason + " samples=" + samples);
    }
    void OnDestroy() { Finish("destroy"); }
}}
