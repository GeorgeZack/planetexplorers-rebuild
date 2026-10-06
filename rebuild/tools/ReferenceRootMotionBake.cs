// Experimental sequential root-motion recovery. No live Animator is changed.
// Only pure Transform+Animator clones receive Update; graphs are retained bounded.
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
    static PoseProbe instance;
    static readonly CultureInfo C = CultureInfo.InvariantCulture;
    sealed class Graph { public GameObject root; public Animator animator; public AnimationClipPlayable playable; }
    static readonly List<Graph> retained = new List<Graph>();
    const long MaxBytes = 4L*1024*1024;
    const int MaxNodes = 512, MaxSamples = 3600;
    StreamWriter output;
    string arm, stop;
    float installed, wait, length, rate;
    int samples;
    long bytes;
    bool finished;
    FieldInfo playerField;
    Graph[] preparedGraphs;
    AnimationClip selectedClip;
    int preparedFrame;
    int activeTrial=-1,trialSample;
    double previousTime;
    float trialStarted;
    float firstSpeed=1f,secondSpeed=.5f,trialSeconds=30f;
    string samplingProfile="standard";
    Vector3 cumulative;
    Quaternion cumulativeRot;
    public static void Install() {
        if(instance!=null)return;
        GameObject host=new GameObject("ReferenceRootMotionBake");
        host.hideFlags=HideFlags.HideAndDontSave; DontDestroyOnLoad(host);
        instance=host.AddComponent<PoseProbe>();
        try {instance.Configure();} catch {Destroy(host);throw;}
    }
    void Configure() {
        string path=Environment.GetEnvironmentVariable("PE_POSE_OUTPUT");
        arm=Environment.GetEnvironmentVariable("PE_POSE_ARM");stop=Environment.GetEnvironmentVariable("PE_POSE_STOP");
        if(String.IsNullOrEmpty(path)||!Path.IsPathRooted(path)||String.IsNullOrEmpty(arm)||String.IsNullOrEmpty(stop))throw new ArgumentException("Absolute output/arm/stop required");
        if(!Single.TryParse(Environment.GetEnvironmentVariable("PE_ROOT_LENGTH"),NumberStyles.Float,C,out length)||Math.Abs(length-1.833339691f)>.00001f)throw new ArgumentException("Clip408 length differs");
        if(!Single.TryParse(Environment.GetEnvironmentVariable("PE_ROOT_RATE"),NumberStyles.Float,C,out rate)||rate!=30)throw new ArgumentException("Clip408 frame rate differs");
        string dense=Environment.GetEnvironmentVariable("PE_ROOT_DENSE");
        if(!String.IsNullOrEmpty(dense)&&dense!="0"&&dense!="1"&&dense!="2"&&dense!="3")throw new ArgumentException("Sampling profile must be0,1,2 or3");
        if(dense=="1"){firstSpeed=.1f;secondSpeed=.05f;trialSeconds=60f;samplingProfile="dense";}
        if(dense=="2"){firstSpeed=.05f;secondSpeed=.025f;trialSeconds=120f;samplingProfile="denser";}
        if(dense=="3"){firstSpeed=.025f;secondSpeed=.0125f;trialSeconds=180f;samplingProfile="ultra-dense";}
        int seconds;
        if(!Int32.TryParse(Environment.GetEnvironmentVariable("PE_POSE_WAIT"),out seconds)||seconds<1||seconds>1800)throw new ArgumentException("Wait bound");wait=seconds;
        foreach(Assembly assembly in AppDomain.CurrentDomain.GetAssemblies()) {
            Type t=assembly.GetType("Pathea.MainPlayerCmpt",false);
            if(t!=null){playerField=t.GetField("gMainPlayer",BindingFlags.Static|BindingFlags.Public);break;}
        }
        if(playerField==null)throw new InvalidOperationException("Main player read field unavailable");
        output=new StreamWriter(new FileStream(path,FileMode.CreateNew,FileAccess.Write,FileShare.Read),new UTF8Encoding(false));
        Write("{\"type\":\"header\",\"schema\":\"pe-root-motion-bake-2\",\"unity\":"+Q(Application.unityVersion)+",\"clip\":\"RunSwordAttack\",\"sourcePathId\":408,\"skillId\":20110014,\"coordinateSpace\":\"Unity clone world XYZ; identity initial root, positive Z forward\",\"playbackSpeeds\":["+N(firstSpeed)+","+N(secondSpeed)+"],\"samplingProfile\":"+Q(samplingProfile)+",\"trialWallLimitSeconds\":"+N(trialSeconds)+",\"applyRootMotion\":true,\"fireEvents\":false,\"controllerAttached\":false,\"initialization\":\"one complete player-loop frame with paused zero-speed pure clones\",\"evaluation\":\"ordinary player-loop Director playback; actual clock checked against scaled frame dt times clone speed\",\"cleanupPolicy\":\"retain two inactive graphs until process exit\"}");
        output.Flush();installed=Time.realtimeSinceStartup;StartCoroutine(Capture());
    }
    IEnumerator Capture() {
        while(!finished) {
            yield return new WaitForEndOfFrame();
            try { Tick(); } catch(Exception e) {Console.Error.WriteLine("[root-bake] "+e);Finish("error");}
        }
        int before=Time.frameCount,alive=0;
        while(alive<120){yield return null;if(Time.frameCount!=before){before=Time.frameCount;alive++;}}
        Console.Error.WriteLine("[root-bake] Post-cleanup alive120frames");Destroy(gameObject);
    }
    void Tick() {
        if(File.Exists(stop)){Finish("stop-file");return;}
        if(preparedGraphs==null&&Time.realtimeSinceStartup-installed>wait){Finish("arm-or-player-timeout");return;}
        if(preparedGraphs!=null) {
            if(Time.frameCount==preparedFrame)return;
            AdvanceFrameTrial();return;
        }
        if(!File.Exists(arm))return;
        Component player=playerField.GetValue(null) as Component;if(player==null)return;
        Transform model=Read(player.GetComponent("BiologyViewCmpt"),"modelTrans") as Transform;if(model==null)return;
        if(Convert.ToString(Read(player.GetComponent("CommonCmpt"),"sex"),C)!="Female")throw new InvalidOperationException("Female source required");
        Animator source=null;
        foreach(Animator a in model.GetComponentsInChildren<Animator>(true)) {
            if(!a.enabled||!a.gameObject.activeInHierarchy||a.avatar==null||!a.avatar.isValid||!a.isHuman||a.avatar.name!="FemaleAvatar")continue;
            if(source!=null)throw new InvalidOperationException("Ambiguous source Animator");source=a;
        }
        if(source==null)return;
        var controller=source.runtimeAnimatorController;
        if(controller==null||controller.name!="FemaleAnimator2")throw new InvalidOperationException("Wrong controller");
        AnimationClip selected=null;AnimationClip[] clips=controller.animationClips;
        if(clips.Length>4096)throw new InvalidOperationException("Clip array bound");
        foreach(AnimationClip clip in clips) {
            if(clip==null||clip.name!="RunSwordAttack"||Math.Abs(clip.length-length)>.0001f||Math.Abs(clip.frameRate-rate)>.001f)continue;
            if(selected!=null&&selected.GetInstanceID()!=clip.GetInstanceID())throw new InvalidOperationException("Ambiguous clip");selected=clip;
        }
        if(selected==null||selected.legacy)throw new InvalidOperationException("Missing humanoid408 clip");
        Transform[] nodes=source.transform.GetComponentsInChildren<Transform>(true);
        if(nodes.Length<2||nodes.Length>MaxNodes)throw new InvalidOperationException("Skeleton bound");
        // Copy once, before either evaluation. Never instantiate source components.
        var positions=new Vector3[nodes.Length];var rotations=new Quaternion[nodes.Length];var scales=new Vector3[nodes.Length];var parents=new int[nodes.Length];var names=new string[nodes.Length];
        var map=new Dictionary<Transform,int>();
        for(int i=0;i<nodes.Length;i++) {
            int parent=-1;if((i==0&&nodes[i]!=source.transform)||(i>0&&!map.TryGetValue(nodes[i].parent,out parent)))throw new InvalidOperationException("Hierarchy order");
            parents[i]=parent;map.Add(nodes[i],i);names[i]=nodes[i].name;
            positions[i]=i==0?Vector3.zero:nodes[i].localPosition;rotations[i]=i==0?Quaternion.identity:nodes[i].localRotation;scales[i]=i==0?Vector3.one:nodes[i].localScale;
        }
        Write("{\"type\":\"source\",\"animatorId\":"+source.GetInstanceID()+",\"clipRuntimeId\":"+selected.GetInstanceID()+",\"avatar\":\"FemaleAvatar\",\"controller\":\"FemaleAnimator2\",\"nodes\":"+nodes.Length+",\"length\":"+N(selected.length)+",\"initialPose\":\"copied live local TRS; root identity; same snapshot for both fresh trials\"}");
        selectedClip=selected;preparedGraphs=new Graph[2];preparedFrame=Time.frameCount;
        for(int trial=0;trial<2;trial++) {
            Graph g=Clone(source.avatar,names,parents,positions,rotations,scales);preparedGraphs[trial]=g;
            g.playable=new AnimationClipPlayable(selected);GC.SuppressFinalize(g.playable);
            g.animator.Play(g.playable);g.animator.SetTimeUpdateMode(DirectorUpdateMode.GameTime);g.playable.time=0;g.playable.speed=0;g.playable.state=PlayState.Paused;
            // Old Director graphs may need one real player-loop initialization.
            // A paused zero-speed frame is diagnostic setup, never motion data.
            g.animator.Update(0f);
        }
        output.Flush();
    }
    void StartTrial(int trial) {
        activeTrial=trial;trialSample=0;previousTime=0;trialStarted=Time.realtimeSinceStartup;
        cumulative=Vector3.zero;cumulativeRot=Quaternion.identity;
        Graph g=preparedGraphs[trial];
        if(Math.Abs(g.playable.time)>.0001)throw new InvalidOperationException("Paused initialization clock drift");
        // Only this clone's speed changes; no global time/FPS or live Animator changes.
        g.playable.speed=trial==0?firstSpeed:secondSpeed;g.playable.state=PlayState.Playing;
        WriteSample(g,trial,0,0,0,0,cumulative,cumulativeRot);trialSample=1;
        output.Flush();
    }
    void AdvanceFrameTrial() {
        if(activeTrial<0){StartTrial(0);return;}
        if(Time.realtimeSinceStartup-trialStarted>trialSeconds)throw new InvalidOperationException("Frame-driven trial wall-time bound");
        Graph g=preparedGraphs[activeTrial];
        float dt=Time.deltaTime;
        if(dt<=0)return; // Global game pause supplies no source-scaled interval.
        if(dt>1)throw new InvalidOperationException("Frame delta outside bound");
        double expected=previousTime+(double)dt*g.playable.speed;
        cumulative+=g.animator.deltaPosition;cumulativeRot=cumulativeRot*g.animator.deltaRotation;
        // No Animator.Update, seek or explicit time write after initialization.
        // The engine must advance its actual clock and move the pure clone.
        WriteSample(g,activeTrial,0,trialSample,expected,dt,cumulative,cumulativeRot);
        previousTime=g.playable.time;trialSample++;output.Flush();
        if(previousTime>=selectedClip.length) {
            Write("{\"type\":\"trialEnd\",\"trial\":"+activeTrial+",\"samples\":"+trialSample+",\"terminalTime\":"+previousTime.ToString("R",C)+"}");
            Inactivate(g);
            if(activeTrial==0)StartTrial(1);else Finish("complete");
        }
    }
    Graph Clone(Avatar avatar,string[] names,int[] parents,Vector3[] p,Quaternion[] q,Vector3[] s) {
        if(retained.Count>=2)throw new InvalidOperationException("Retained graph bound");
        var g=new Graph();g.root=new GameObject("ReferenceRootMotionSkeleton_"+retained.Count);retained.Add(g);
        g.root.hideFlags=HideFlags.HideAndDontSave;g.root.SetActive(false);DontDestroyOnLoad(g.root);
        var bones=new Transform[names.Length];
        for(int i=0;i<names.Length;i++) {
            if(i==0)bones[i]=g.root.transform;
            else {var child=new GameObject(names[i]);child.hideFlags=HideFlags.HideAndDontSave;bones[i]=child.transform;bones[i].parent=bones[parents[i]];}
            bones[i].localPosition=p[i];bones[i].localRotation=q[i];bones[i].localScale=s[i];
        }
        g.animator=g.root.AddComponent<Animator>();g.animator.avatar=avatar;g.animator.runtimeAnimatorController=null;
        g.animator.fireEvents=false;g.animator.applyRootMotion=true;g.animator.cullingMode=AnimatorCullingMode.AlwaysAnimate;g.animator.speed=1;
        g.root.SetActive(true);g.animator.Rebind();
        if(!g.animator.isHuman||!g.animator.avatar.isValid)throw new InvalidOperationException("Clone binding invalid");
        foreach(Component c in g.root.GetComponentsInChildren<Component>(true))if(!(c is Transform)&&c!=g.animator)throw new InvalidOperationException("Unexpected clone component");
        return g;
    }
    void WriteSample(Graph g,int trial,int fps,int index,double expected,float dt,Vector3 sum,Quaternion sumRot) {
        if(samples>=MaxSamples||g.animator.runtimeAnimatorController!=null||g.animator.fireEvents||!g.animator.applyRootMotion)throw new InvalidOperationException("Bounds/isolation changed");
        if(Math.Abs(g.playable.time-expected)>.0001) {
            var diagnostic=new StringBuilder("{\"type\":\"clockFailure\",\"trial\":");
            diagnostic.Append(trial).Append(",\"index\":").Append(index).Append(",\"frame\":").Append(Time.frameCount);
            diagnostic.Append(",\"expected\":").Append(expected.ToString("R",C)).Append(",\"actual\":").Append(g.playable.time.ToString("R",C));
            diagnostic.Append(",\"dt\":").Append(N(dt)).Append(",\"playState\":").Append(Q(g.playable.state.ToString())).Append(",\"speed\":").Append(N(g.playable.speed));
            diagnostic.Append(",\"position\":");V(diagnostic,g.root.transform.position);
            diagnostic.Append(",\"deltaPosition\":");V(diagnostic,g.animator.deltaPosition);
            Write(diagnostic.Append('}').ToString());output.Flush();
            throw new InvalidOperationException("Sequential playable clock differs: expected="+expected.ToString("R",C)+" actual="+g.playable.time.ToString("R",C)+" dt="+N(dt));
        }
        var b=new StringBuilder("{\"type\":\"sample\",\"trial\":");b.Append(trial).Append(",\"playbackSpeed\":").Append(N(g.playable.speed)).Append(",\"index\":").Append(index).Append(",\"cloneId\":").Append(g.root.GetInstanceID());
        b.Append(",\"directorTime\":").Append(g.animator.GetTime().ToString("R",C)).Append(",\"directorMode\":").Append(Q(g.animator.GetTimeUpdateMode().ToString()));
        b.Append(",\"frame\":").Append(Time.frameCount).Append(",\"time\":").Append(expected.ToString("R",C)).Append(",\"playableTime\":").Append(g.playable.time.ToString("R",C)).Append(",\"dt\":").Append(N(dt));
        b.Append(",\"position\":");V(b,g.root.transform.position);b.Append(",\"rotation\":");Q4(b,g.root.transform.rotation);
        b.Append(",\"deltaPosition\":");V(b,g.animator.deltaPosition);b.Append(",\"deltaRotation\":");Q4(b,g.animator.deltaRotation);
        b.Append(",\"cumulativeDeltaPosition\":");V(b,sum);b.Append(",\"cumulativeDeltaRotation\":");Q4(b,sumRot);
        b.Append(",\"animatorRootPosition\":");V(b,g.animator.rootPosition);b.Append(",\"animatorRootRotation\":");Q4(b,g.animator.rootRotation);
        Write(b.Append('}').ToString());samples++;
    }
    static void Inactivate(Graph g){if(g.animator!=null)g.animator.enabled=false;if(g.root!=null)g.root.SetActive(false);}
    void Finish(string reason) {
        if(finished)return;finished=true;foreach(Graph g in retained)Inactivate(g);
        if(output!=null){try{Write("{\"type\":\"end\",\"reason\":"+Q(reason)+",\"samples\":"+samples+"}");}finally{output.Dispose();output=null;}}
        Console.Error.WriteLine("[root-bake] Finished "+reason+" samples="+samples);
    }
    void OnApplicationQuit(){Finish("application-quit");}
    void OnDestroy(){Finish("destroy");if(instance==this)instance=null;}
    void Write(string line){int size=Encoding.UTF8.GetByteCount(line)+1;if(size>16384||bytes+size>MaxBytes)throw new InvalidOperationException("Output bound");output.WriteLine(line);bytes+=size;}
    static string N(float f){if(Single.IsNaN(f)||Single.IsInfinity(f))throw new InvalidOperationException("Nonfinite value");return f.ToString("R",C);}
    static string Q(string s){return "\""+s.Replace("\\","\\\\").Replace("\"","\\\"")+"\"";}
    static void V(StringBuilder b,Vector3 v){b.Append('[').Append(N(v.x)).Append(',').Append(N(v.y)).Append(',').Append(N(v.z)).Append(']');}
    static void Q4(StringBuilder b,Quaternion q){b.Append('[').Append(N(q.x)).Append(',').Append(N(q.y)).Append(',').Append(N(q.z)).Append(',').Append(N(q.w)).Append(']');}
    static object Read(object o,string n){if(o==null)return null;var t=o.GetType();var p=t.GetProperty(n,BindingFlags.Public|BindingFlags.Instance);if(p!=null)return p.GetValue(o,null);var f=t.GetField(n,BindingFlags.Public|BindingFlags.Instance);return f==null?null:f.GetValue(o);}
}
}
