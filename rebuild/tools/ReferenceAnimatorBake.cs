// Isolated authored-clip sampler for the installed Unity 5.2 player.
// The entry name matches the existing launch-only main-thread shim. This DLL
// replaces neither the observer DLL nor any original game file.
using System;
using System.IO;
using System.Text;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using UnityEngine;
using UnityEngine.Experimental.Director;

namespace PeReference {
public sealed class PoseProbe : MonoBehaviour {
    const long MaxBytes = 256L * 1024 * 1024;
    const int MaxTransforms = 512, MaxClips = 32, MaxSamples = 3600;
    static readonly CultureInfo Invariant = CultureInfo.InvariantCulture;
    static PoseProbe instance;
    static readonly string[] Allowed = {"NormalIdle", "NormalWF", "NormalRF", "Jump", "Fall", "Land", "Attack6", "Attack7", "Attack8", "Attack9", "Felling1", "Felling2", "Felling3", "Spade1", "Spade2", "Spade3", "Mine1", "Mine2", "Mine3", "AttackIdle", "SwordWF", "SwordRF", "SwordWB", "SwordRB", "SwordSprint", "SwordJump", "SwordFall", "SwordLand", "SwordRunJump", "SwordRunFall", "SwordRunLand", "NormalWB", "NormalWL", "NormalWR", "NormalRB", "NormalRL", "NormalRR", "NormalWalkTurnLeft", "NormalWalkTurnRight", "NormalRunTurnLeft", "NormalRunTurnRight", "RunSwordAttack",
        "IdleBeHit", "SwordBeHit", "ForwardFast", "FowardRightFast", "RightFast", "BackRightFast", "BackFast", "ForwardLeftFast", "LeftFast", "BackLeftFast", "Forward", "ForwardRight", "Right", "BackRight", "Back", "ForwardLeft", "Left", "BackLeft", "BlockRepulsed",
        "GetUpBackMirror", "GetUpFrontMirror", "GetUpBack", "GetUpFront", "SprintSwordAttack",
        "HerbDown", "Herb", "HerbUp"};
    sealed class ClipSpec { public string name; public float length, rate; public int sourceId; public bool loop; public AnimationClip clip; }
    sealed class RetainedGraph { public GameObject root; public Animator animator; public AnimationClipPlayable playable; }
    static readonly List<RetainedGraph> retained = new List<RetainedGraph>();
    RetainedGraph owned;
    ClipSpec[] clips;
    StreamWriter output;
    string arm, stop, expectedAvatar, expectedController, expectedGender;
    float installedAt, startedAt = -1, waitSeconds, bakeSeconds;
    int sampleRate, frames, clipIndex = -1, clipSample, clipFrames;
    long bytes;
    bool finished, pending;
    FieldInfo playerField;
    int sourceEntity;
    Type entityManagerType;
    GameObject clone;
    Animator animator;
    AnimationClipPlayable playable;
    Transform[] bones;
    string[] paths, sourceNames;
    int[] sourceIds, parents;
    Avatar sourceAvatar;
    Vector3[] initialPosition, initialScale;
    Quaternion[] initialRotation;

    public static void Install() {
        if (instance != null) return;
        string path = Environment.GetEnvironmentVariable("PE_POSE_OUTPUT");
        if (String.IsNullOrEmpty(path) || !Path.IsPathRooted(path)) throw new ArgumentException("Absolute bake output required");
        GameObject host = new GameObject("ReferenceAnimatorBake (isolated clip sampler)");
        host.hideFlags = HideFlags.HideAndDontSave;
        DontDestroyOnLoad(host);
        instance = host.AddComponent<PoseProbe>();
        try { instance.Configure(path); } catch { Destroy(host); throw; }
    }
    static int Setting(string name, int fallback, int min, int max) {
        string s = Environment.GetEnvironmentVariable(name); int n;
        if (String.IsNullOrEmpty(s)) return fallback;
        if (!Int32.TryParse(s, out n) || n < min || n > max) throw new ArgumentException(name);
        return n;
    }
    void Configure(string path) {
        arm = Environment.GetEnvironmentVariable("PE_POSE_ARM"); stop = Environment.GetEnvironmentVariable("PE_POSE_STOP");
        expectedAvatar = Environment.GetEnvironmentVariable("PE_BAKE_AVATAR");
        expectedController = Environment.GetEnvironmentVariable("PE_BAKE_CONTROLLER");
        expectedGender = Environment.GetEnvironmentVariable("PE_BAKE_GENDER");
        if (String.IsNullOrEmpty(arm) || String.IsNullOrEmpty(stop) || String.IsNullOrEmpty(expectedAvatar) || String.IsNullOrEmpty(expectedController))
            throw new ArgumentException("Missing bake configuration");
        if (expectedGender != "Female" && expectedGender != "Male") throw new ArgumentException("Unsupported gender");
        sourceEntity = Setting("PE_BAKE_SOURCE_ENTITY", 0, 1, 1000000);
        sampleRate = Setting("PE_BAKE_RATE", 30, 1, 60);
        waitSeconds = Setting("PE_POSE_WAIT", 900, 1, 1800);
        bakeSeconds = Setting("PE_BAKE_SECONDS", 180, 1, 600);
        string spec = Environment.GetEnvironmentVariable("PE_BAKE_CLIPS");
        if (String.IsNullOrEmpty(spec) || spec.Length > 4096) throw new ArgumentException("Missing clip plan");
        string[] rows = spec.Split(';');
        if (rows.Length < 1 || rows.Length > MaxClips) throw new ArgumentException("Clip plan bound");
        clips = new ClipSpec[rows.Length]; HashSet<string> seen = new HashSet<string>();
        for (int i = 0; i < rows.Length; i++) {
            string[] fields = rows[i].Split('|'); float length, rate; int id;
            if (fields.Length != 5 || Array.IndexOf(Allowed, fields[0]) < 0 || !seen.Add(fields[0]) ||
                !Single.TryParse(fields[1], NumberStyles.Float, Invariant, out length) || !Single.TryParse(fields[2], NumberStyles.Float, Invariant, out rate) ||
                !Int32.TryParse(fields[3], out id) || !Finite(length) || length <= 0 || length > 10 || !Finite(rate) || rate <= 0 || rate > 120 || id <= 0 || (fields[4]!="0"&&fields[4]!="1"))
                throw new ArgumentException("Invalid clip plan row");
            clips[i] = new ClipSpec {name=fields[0], length=length, rate=rate, sourceId=id, loop=fields[4]=="1"};
        }
        foreach (Assembly assembly in AppDomain.CurrentDomain.GetAssemblies()) {
            Type t = assembly.GetType("Pathea.MainPlayerCmpt", false);
            if (t != null) { playerField = t.GetField("gMainPlayer", BindingFlags.Public | BindingFlags.Static); break; }
        }
        if(sourceEntity != 0) {
            foreach(Assembly assembly in AppDomain.CurrentDomain.GetAssemblies()) {
                Type t=assembly.GetType("Pathea.EntityMgr",false);
                if(t!=null){entityManagerType=t;break;}
            }
            if(entityManagerType==null)throw new InvalidOperationException("EntityMgr unavailable");
        }
        if (playerField == null) throw new InvalidOperationException("MainPlayerCmpt unavailable");
        output = new StreamWriter(new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read), new UTF8Encoding(false));
        installedAt = Time.realtimeSinceStartup;
        Write("{\"type\":\"header\",\"schema\":\"pe-animator-bake-1\",\"unity\":" + Q(Application.unityVersion) +
              ",\"matrixOrder\":\"row-major\",\"coordinateSpace\":\"isolated-animator-model-local\",\"sampleRate\":" + sampleRate +
              ",\"sampleCountPolicy\":\"ceil-f64-product-terminal-length\",\"cleanupPolicy\":\"retain-inactive-per-clip-until-process-exit\",\"maxRetainedGraphs\":" + MaxClips + ",\"postCleanupUpdates\":120" +
              ",\"maxBytes\":" + MaxBytes + ",\"maxSamples\":" + MaxSamples + ",\"fireEvents\":false,\"controllerAttached\":false,\"applyRootMotion\":false}");
        output.Flush(); Console.Error.WriteLine("[bake] Waiting for arm file and matching live player: " + arm);
        StartCoroutine(Capture());
    }
    IEnumerator Capture() {
        while (!finished) {
            yield return new WaitForEndOfFrame();
            try { Tick(); } catch (Exception e) { Console.Error.WriteLine("[bake] Error: " + e); Finish("error"); }
        }
        // Output is closed and every used graph/hierarchy is inactive, retained
        // in a bounded static list until this disposable process terminates.
        // Keep only this observer alive to establish continued player updates
        // after cleanup, rather than inferring survival from wall-clock delay.
        int previousFrame=Time.frameCount, alive=0;
        while(alive<120) { yield return null; if(Time.frameCount!=previousFrame){previousFrame=Time.frameCount;alive++;} }
        Console.Error.WriteLine("[bake] Post-cleanup alive120frames");
        Destroy(gameObject);
    }
    Component SourceEntity() {
        if(sourceEntity==0)return playerField.GetValue(null) as Component;
        PropertyInfo singleton=entityManagerType.GetProperty("Instance",BindingFlags.Public|BindingFlags.Static|BindingFlags.FlattenHierarchy);
        MethodInfo get=entityManagerType.GetMethod("Get",new Type[]{typeof(int)});
        if(singleton==null||get==null)throw new InvalidOperationException("EntityMgr read API unavailable");
        object manager=singleton.GetValue(null,null);
        if(manager==null)return null;
        return get.Invoke(manager,new object[]{sourceEntity}) as Component;
    }
    void Tick() {
        float now = Time.realtimeSinceStartup;
        if (File.Exists(stop)) { Finish("stop-file"); return; }
        if (startedAt < 0 && now-installedAt >= waitSeconds) { Finish("arm-or-player-timeout"); return; }
        if (startedAt >= 0 && (now-startedAt >= bakeSeconds || frames >= MaxSamples)) { Finish("bake-limit"); return; }
        if (animator == null) {
            if (!File.Exists(arm)) return;
            Component player = SourceEntity();
            if (player == null) return;
            Transform model = Read(player.GetComponent("BiologyViewCmpt"), "modelTrans") as Transform;
            if (model == null) return;
            string gender = Convert.ToString(Read(player.GetComponent("CommonCmpt"), "sex"), Invariant);
            if (gender != expectedGender) throw new InvalidOperationException("Live player gender differs from selected plan");
            Animator selected = null;
            foreach (Animator candidate in model.GetComponentsInChildren<Animator>(true)) {
                if (!candidate.enabled || !candidate.gameObject.activeInHierarchy || candidate.avatar == null || !candidate.avatar.isValid || !candidate.isHuman) continue;
                if (candidate.avatar.name != expectedAvatar) continue;
                if (selected != null) throw new InvalidOperationException("Ambiguous player Animator");
                selected = candidate;
            }
            if (selected == null) return;
            Prepare(selected, model, gender);
            startedAt = now; BeginClip(); return;
        }
        // The target time was set after the previous frame's evaluation. One
        // full player-loop frame now elapsed with the playable paused. Capture
        // that pose, then seek the next sample. No dependence on render FPS.
        if (pending) {
            WritePose(); pending = false; frames++; clipSample++;
            if (clipSample >= clipFrames) {
                Write("{\"type\":\"clipEnd\",\"clip\":" + clipIndex + ",\"samples\":" + clipFrames + "}");
                output.Flush(); BeginClip(); return;
            }
        }
        Seek();
    }
    void Prepare(Animator original, Transform model, string gender) {
        RuntimeAnimatorController controller = original.runtimeAnimatorController;
        if (controller == null || controller.name != expectedController) throw new InvalidOperationException("Unexpected live player controller");
        AnimationClip[] available = controller.animationClips;
        if (available.Length > 4096) throw new InvalidOperationException("Controller clip bound");
        int planned = 0;
        foreach (ClipSpec spec in clips) {
            foreach (AnimationClip clip in available) {
                if (clip == null || clip.name != spec.name) continue;
                if (Math.Abs(clip.length-spec.length) > 0.0001f || Math.Abs(clip.frameRate-spec.rate) > 0.001f) continue;
                if (spec.clip != null && spec.clip.GetInstanceID() != clip.GetInstanceID()) throw new InvalidOperationException("Ambiguous source clip: " + spec.name);
                spec.clip = clip;
            }
            // Motion.isHumanMotion/isLooping compile but are unavailable in this
            // shipped native player. Humanoid bindings/loop come from the
            // launcher's exact source-asset-SHA-bound inventory instead.
            if (spec.clip == null || spec.clip.legacy) throw new InvalidOperationException("Missing or unsupported source clip: " + spec.name);
            planned += (int)Math.Ceiling((double)spec.clip.length*sampleRate)+1;
        }
        if (planned > MaxSamples) throw new InvalidOperationException("Planned samples exceed limit");
        Transform[] sources = original.transform.GetComponentsInChildren<Transform>(true);
        if (sources.Length < 2 || sources.Length > MaxTransforms) throw new InvalidOperationException("Skeleton transform bound");
        paths = new string[sources.Length]; sourceNames=new string[sources.Length]; sourceIds=new int[sources.Length]; parents=new int[sources.Length];
        initialPosition = new Vector3[sources.Length]; initialRotation = new Quaternion[sources.Length]; initialScale = new Vector3[sources.Length];
        Dictionary<Transform,int> map = new Dictionary<Transform,int>(); HashSet<string> names = new HashSet<string>();
        for (int i = 0; i < sources.Length; i++) {
            Transform source = sources[i]; int parent=-1;
            if(i==0 && source!=original.transform)throw new InvalidOperationException("Source root order invalid");
            if(i>0 && !map.TryGetValue(source.parent,out parent))throw new InvalidOperationException("Source hierarchy order/parent invalid");
            parents[i]=parent;sourceNames[i]=source.name;sourceIds[i]=source.GetInstanceID();map.Add(source,i);
            paths[i] = Relative(source, original.transform);
            if (!names.Add(paths[i])) throw new InvalidOperationException("Duplicate relative transform path");
            initialPosition[i] = i == 0 ? Vector3.zero : source.localPosition;
            initialRotation[i] = i == 0 ? Quaternion.identity : source.localRotation;
            initialScale[i] = i == 0 ? Vector3.one : source.localScale;
        }
        sourceAvatar=original.avatar;CreateClone();
        StringBuilder b = new StringBuilder("{\"type\":\"skeleton\",\"gender\":"); b.Append(Q(gender));
        b.Append(",\"sourceEntityId\":").Append(sourceEntity);
        b.Append(",\"sourceModelPath\":").Append(Q(FullPath(model))).Append(",\"sourceAnimatorPath\":").Append(Q(FullPath(original.transform)));
        b.Append(",\"sourceAnimatorId\":").Append(original.GetInstanceID()).Append(",\"sourceController\":").Append(Q(controller.name));
        b.Append(",\"avatar\":").Append(Q(original.avatar.name)).Append(",\"avatarId\":").Append(original.avatar.GetInstanceID());
        b.Append(",\"initialPose\":\"copied live local TRS; fresh pure clone per clip\",\"cloneIdsDescribeFirstClip\":true,\"plannedSamples\":").Append(planned).Append(",\"nodes\":[");
        for (int i=0;i<bones.Length;i++) { if(i>0)b.Append(',');
            b.Append("{\"path\":").Append(Q(paths[i])).Append(",\"sourceId\":").Append(sourceIds[i]).Append(",\"cloneId\":").Append(bones[i].GetInstanceID());
            b.Append(",\"parent\":").Append(parents[i]);
            b.Append(",\"initialLocal\":"); Matrix(b,Matrix4x4.TRS(initialPosition[i],initialRotation[i],initialScale[i])); b.Append('}'); }
        b.Append("],\"humanBones\":{"); bool first=true;
        for(int i=0;i<(int)HumanBodyBones.LastBone;i++) {Transform t=animator.GetBoneTransform((HumanBodyBones)i);if(t==null)continue;
            if(!first)b.Append(',');first=false;b.Append(Q(((HumanBodyBones)i).ToString())).Append(':').Append(Array.IndexOf(bones,t));}
        Write(b.Append("}}").ToString()); output.Flush();
        Console.Error.WriteLine("[bake] Isolated " + bones.Length + " transforms; " + clips.Length + " clips / " + planned + " samples");
    }
    void CreateClone() {
        if(retained.Count>=MaxClips)throw new InvalidOperationException("Retained graph bound exceeded");
        clone=new GameObject("ReferenceAnimatorBakeSkeleton_"+retained.Count);clone.hideFlags=HideFlags.HideAndDontSave;
        clone.SetActive(false);DontDestroyOnLoad(clone);
        owned=new RetainedGraph{root=clone};retained.Add(owned);
        bones=new Transform[paths.Length];
        for(int i=0;i<bones.Length;i++) {
            Transform t;
            if(i==0)t=clone.transform;
            else {GameObject child=new GameObject(sourceNames[i]);child.hideFlags=HideFlags.HideAndDontSave;t=child.transform;t.parent=bones[parents[i]];}
            bones[i]=t;t.localPosition=initialPosition[i];t.localRotation=initialRotation[i];t.localScale=initialScale[i];
        }
        animator=clone.AddComponent<Animator>();owned.animator=animator;
        animator.avatar=sourceAvatar;animator.runtimeAnimatorController=null;animator.fireEvents=false;animator.applyRootMotion=false;
        animator.cullingMode=AnimatorCullingMode.AlwaysAnimate;animator.speed=1;
        clone.SetActive(true);animator.Rebind();
        if(!animator.isHuman||animator.avatar==null||!animator.avatar.isValid)throw new InvalidOperationException("Isolated Avatar binding invalid");
    }
    void BeginClip() {
        if(playable!=null)RetainInactive();
        clipIndex++;
        if (clipIndex >= clips.Length) { Finish("complete"); return; }
        if(clone==null)CreateClone();
        AnimationClip clip = clips[clipIndex].clip;
        playable = new AnimationClipPlayable(clip); owned.playable=playable;
        // The installed Playable base type has a finalizer that releases its
        // native pointer. Keep it strongly rooted and suppress that finalizer;
        // Unity owns process-end native graph shutdown in this bounded mode.
        GC.SuppressFinalize(playable);
        animator.Play(playable);
        playable.state = PlayState.Paused; playable.speed = 0;
        clipSample = 0; clipFrames = (int)Math.Ceiling((double)clip.length*sampleRate)+1;
        StringBuilder b = new StringBuilder("{\"type\":\"clip\",\"index\":"); b.Append(clipIndex).Append(",\"name\":").Append(Q(clip.name));
        b.Append(",\"sourcePathId\":").Append(clips[clipIndex].sourceId).Append(",\"runtimeId\":").Append(clip.GetInstanceID());
        b.Append(",\"length\":").Append(N(clip.length)).Append(",\"sourceFrameRate\":").Append(N(clip.frameRate)).Append(",\"samples\":").Append(clipFrames);
        b.Append(",\"cloneIds\":[");for(int i=0;i<bones.Length;i++){if(i>0)b.Append(',');b.Append(bones[i].GetInstanceID());}b.Append(']');
        b.Append(",\"isLooping\":").Append(clips[clipIndex].loop?"true":"false").Append(",\"events\":[");
        AnimationEvent[] events=clip.events;if(events.Length>512)throw new InvalidOperationException("Event bound");
        for(int i=0;i<events.Length;i++){if(i>0)b.Append(',');b.Append("{\"time\":").Append(N(events[i].time)).Append(",\"function\":").Append(Q(events[i].functionName)).Append(",\"stringParameter\":").Append(Q(events[i].stringParameter)).Append('}');}
        Write(b.Append("]}").ToString()); Seek();
    }
    double SampleTime() { return clipSample==clipFrames-1 ? clips[clipIndex].clip.length : Math.Min((double)clipSample/sampleRate,clips[clipIndex].clip.length); }
    void Seek() { playable.time=SampleTime(); animator.Update(0f); pending=true; }
    void WritePose() {
        if (animator.runtimeAnimatorController != null || animator.fireEvents || animator.applyRootMotion) throw new InvalidOperationException("Isolation flags changed");
        Component[] components=clone.GetComponentsInChildren<Component>(true);
        foreach(Component c in components)if(!(c is Transform)&&c!=animator)throw new InvalidOperationException("Unexpected component on isolated skeleton");
        if (Math.Abs(playable.time-SampleTime())>0.00001) throw new InvalidOperationException("Paused playable clock drift");
        StringBuilder b=new StringBuilder("{\"type\":\"pose\",\"clip\":");b.Append(clipIndex).Append(",\"sample\":").Append(clipSample).Append(",\"frame\":").Append(Time.frameCount);
        b.Append(",\"authoredTime\":").Append(SampleTime().ToString("R",Invariant)).Append(",\"realtime\":").Append(N(Time.realtimeSinceStartup));
        b.Append(",\"rootWorld\":");Matrix(b,clone.transform.localToWorldMatrix);
        b.Append(",\"animatorRootPosition\":");Vector(b,animator.rootPosition);b.Append(",\"animatorRootRotation\":");Rotation(b,animator.rootRotation);
        b.Append(",\"deltaPosition\":");Vector(b,animator.deltaPosition);b.Append(",\"deltaRotation\":");Rotation(b,animator.deltaRotation);
        b.Append(",\"local\":[");for(int i=0;i<bones.Length;i++){if(i>0)b.Append(',');Transform t=bones[i];Matrix(b,Matrix4x4.TRS(t.localPosition,t.localRotation,t.localScale));}
        b.Append("],\"world\":[");for(int i=0;i<bones.Length;i++){if(i>0)b.Append(',');Matrix(b,bones[i].localToWorldMatrix);}Write(b.Append("]}").ToString());
    }
    static bool Finite(float x){return !Single.IsNaN(x)&&!Single.IsInfinity(x);}
    static string N(float f){if(!Finite(f))throw new InvalidOperationException("Nonfinite pose");return f.ToString("R",Invariant);}
    static string Q(string s){if(s==null)return "null";StringBuilder b=new StringBuilder("\"");foreach(char c in s){if(c=='"'||c=='\\')b.Append('\\').Append(c);else if(c<32)b.Append("\\u").Append(((int)c).ToString("x4"));else b.Append(c);}return b.Append('"').ToString();}
    static object Read(object o,string n){if(o==null)return null;Type t=o.GetType();PropertyInfo p=t.GetProperty(n,BindingFlags.Public|BindingFlags.Instance);if(p!=null)return p.GetValue(o,null);FieldInfo f=t.GetField(n,BindingFlags.Public|BindingFlags.Instance);return f==null?null:f.GetValue(o);}
    static string FullPath(Transform t){string s=t.name;int depth=0;while(t.parent!=null){if(++depth>128)throw new InvalidOperationException("Path depth");t=t.parent;s=t.name+"/"+s;}return s;}
    static string Relative(Transform t,Transform root){if(t==root)return "";string s=t.name;int depth=0;while(t.parent!=root){if(t.parent==null||++depth>128)throw new InvalidOperationException("Relative path invalid");t=t.parent;s=t.name+"/"+s;}return s;}
    static void Matrix(StringBuilder b,Matrix4x4 m){b.Append('[');for(int r=0;r<4;r++)for(int c=0;c<4;c++){if(r!=0||c!=0)b.Append(',');b.Append(N(m[r,c]));}b.Append(']');}
    static void Vector(StringBuilder b,Vector3 v){b.Append('[').Append(N(v.x)).Append(',').Append(N(v.y)).Append(',').Append(N(v.z)).Append(']');}
    static void Rotation(StringBuilder b,Quaternion q){b.Append('[').Append(N(q.x)).Append(',').Append(N(q.y)).Append(',').Append(N(q.z)).Append(',').Append(N(q.w)).Append(']');}
    void Write(string s){int size=Encoding.UTF8.GetByteCount(s)+1;if(size>4*1024*1024||bytes+size>MaxBytes)throw new InvalidOperationException("Output bound exceeded");output.WriteLine(s);bytes+=size;}
    void RetainInactive() {
        if(clone==null)return;
        if(animator!=null)animator.enabled=false;
        clone.SetActive(false);
        // No Stop, Dispose, Destroy or Rebind on a used native graph. Two
        // disposal orders crashed this original runtime after valid sampling.
        clone=null;animator=null;playable=null;owned=null;bones=null;
        Console.Error.WriteLine("[bake] Retained inactive graph until process exit; total="+retained.Count);
    }
    void Finish(string reason){if(finished)return;finished=true;pending=false;
        try { RetainInactive(); }
        catch(Exception e){Console.Error.WriteLine("[bake] Inactivation failed: "+e.Message);reason="cleanup-error";}
        if(output!=null){try{if(bytes<MaxBytes-1024)Write("{\"type\":\"end\",\"reason\":"+Q(reason)+",\"samples\":"+frames+"}");}catch(Exception e){Console.Error.WriteLine("[bake] Final record: "+e.Message);}finally{try{output.Dispose();}catch(Exception e){Console.Error.WriteLine("[bake] Close: "+e.Message);}output=null;}}
        Console.Error.WriteLine("[bake] Finished: "+reason+"; samples="+frames);
    }
    void OnApplicationQuit(){Finish("application-quit");}
    void OnDestroy(){Finish("destroy");if(instance==this)instance=null;}
}
}
