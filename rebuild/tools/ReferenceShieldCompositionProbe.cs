// Separate, bounded launch-only probe. Never attaches a controller to a live object.
using System;
using System.IO;
using System.Text;
using System.Collections;
using System.Reflection;
using System.Globalization;
using UnityEngine;

namespace PeReference {
// The established main-thread shim invokes PeReference.PoseProbe.Install.
// This is a separate assembly/output directory, not the ordinary observer DLL.
public sealed class PoseProbe : MonoBehaviour {
    static PoseProbe instance;
    static readonly CultureInfo Inv=CultureInfo.InvariantCulture;
    StreamWriter output;
    string arm,stop,bundle;
    FieldInfo playerField;
    ShieldCompositionClone clone;
    float installed,started=-1;
    int index,frames;
    long bytes;
    bool finished,pending;
    public static void Install(){
        if(instance!=null)return;
        GameObject host=new GameObject("ReferenceShieldCompositionProbe");host.hideFlags=HideFlags.HideAndDontSave;
        DontDestroyOnLoad(host);instance=host.AddComponent<PoseProbe>();
        try{instance.Configure();}catch{Destroy(host);throw;}
    }
    void Configure(){
        string path=Environment.GetEnvironmentVariable("PE_POSE_OUTPUT");arm=Environment.GetEnvironmentVariable("PE_POSE_ARM");
        stop=Environment.GetEnvironmentVariable("PE_POSE_STOP");bundle=Environment.GetEnvironmentVariable("PE_SHIELD_BUNDLE");
        foreach(string p in new string[]{path,arm,stop,bundle})if(String.IsNullOrEmpty(p)||!Path.IsPathRooted(p))throw new ArgumentException("Absolute probe paths required");
        if(ShieldReplayPlan.Samples.Length!=3)throw new InvalidOperationException("Three-frame bound");
        foreach(Assembly assembly in AppDomain.CurrentDomain.GetAssemblies()){
            Type t=assembly.GetType("Pathea.MainPlayerCmpt",false);
            if(t!=null){playerField=t.GetField("gMainPlayer",BindingFlags.Public|BindingFlags.Static);break;}
        }
        if(playerField==null)throw new InvalidOperationException("Player read API unavailable");
        output=new StreamWriter(new FileStream(path,FileMode.CreateNew,FileAccess.Write,FileShare.Read),new UTF8Encoding(false));
        installed=Time.realtimeSinceStartup;
        Write("{\"type\":\"header\",\"schema\":\"pe-shield-composition-1\",\"unity\":"+Q(Application.unityVersion)+
              ",\"planSha256\":"+Q(ShieldReplayPlan.PlanHash)+",\"bundleSha256\":"+Q(ShieldReplayPlan.BundleHash)+
              ",\"maxSamples\":3,\"maxSeconds\":30,\"maxBytes\":4194304,\"cleanup\":\"retain-inactive-until-exit\",\"postCleanupFrames\":120}");
        StartCoroutine(Capture());
    }
    IEnumerator Capture(){
        while(!finished){yield return new WaitForEndOfFrame();try{Tick();}catch(Exception e){Console.Error.WriteLine("[shield-composition] "+e);Finish("error",e.ToString());}}
        int previous=Time.frameCount,alive=0;
        while(alive<120){yield return null;if(Time.frameCount!=previous){previous=Time.frameCount;alive++;}}
        Console.Error.WriteLine("[shield-composition] Post-cleanup alive120frames");Destroy(gameObject);
    }
    static object Read(Component c,string name){
        if(c==null)return null;Type t=c.GetType();int b=BindingFlags.Public|BindingFlags.NonPublic|BindingFlags.Instance;
        FieldInfo f=t.GetField(name,b);if(f!=null)return f.GetValue(c);PropertyInfo p=t.GetProperty(name,b);return p==null?null:p.GetValue(c,null);
    }
    void Tick(){
        if(File.Exists(stop)){Finish("stop-file",null);return;}
        float now=Time.realtimeSinceStartup;
        if(started<0&&now-installed>=900){Finish("arm-timeout",null);return;}
        if(started>=0&&now-started>=30){Finish("time-limit",null);return;}
        if(clone==null){
            if(!File.Exists(arm))return;
            Component player=playerField.GetValue(null) as Component;if(player==null)return;
            Transform model=Read(player.GetComponent("BiologyViewCmpt"),"modelTrans") as Transform;if(model==null)return;
            if(Convert.ToString(Read(player.GetComponent("CommonCmpt"),"sex"),Inv)!="Female")throw new InvalidOperationException("Female player required");
            Animator source=null;
            foreach(Animator a in model.GetComponentsInChildren<Animator>(true)){
                if(!a.enabled||!a.gameObject.activeInHierarchy||a.avatar==null||a.avatar.name!="FemaleAvatar"||!a.isHuman)continue;
                if(source!=null)throw new InvalidOperationException("Ambiguous source Animator");source=a;
            }
            if(source==null)return;
            if(source.runtimeAnimatorController==null||source.runtimeAnimatorController.name!="FemaleAnimator2")throw new InvalidOperationException("Wrong source controller");
            started=now;
            // Live source is only read. Its controller is never assigned anywhere.
            clone=ShieldCompositionClone.Create(source,bundle,ShieldReplayPlan.BundleHash,ShieldReplayPlan.Clips,Diagnostic);
            Write("{\"type\":\"cloneReady\",\"sourceAvatar\":"+Q(source.avatar.name)+",\"sourceController\":"+Q(source.runtimeAnimatorController.name)+
                  ",\"bones\":"+clone.bones.Length+",\"layers\":"+clone.animator.layerCount+"}");
            Seek();return;
        }
        if(!pending)throw new InvalidOperationException("Missing pending replay");
        WritePose();pending=false;frames++;index++;
        if(index==3){Finish("complete",null);return;}
        Seek();
    }
    void Diagnostic(string message){Write("{\"type\":\"diagnostic\",\"message\":"+Q(message)+"}");Console.Error.WriteLine("[shield-composition] "+message);}
    void Seek(){clone.Seek(ShieldReplayPlan.Parameters[index],ShieldReplayPlan.BaseTimes[index],ShieldReplayPlan.BlockTimes[index]);pending=true;}
    static string Q(string s){if(s==null)return "null";return "\""+s.Replace("\\","\\\\").Replace("\"","\\\"").Replace("\n","\\n").Replace("\r","\\r").Replace("\t","\\t")+"\"";}
    static string N(float x,string context){return ShieldCaptureNumbers.Finite(x,context);}
    static string Matrix(Matrix4x4 m,string context){StringBuilder b=new StringBuilder("[");for(int r=0;r<4;r++)for(int c=0;c<4;c++){if(r!=0||c!=0)b.Append(',');b.Append(N(m[r,c],context+"["+r+","+c+"]"));}return b.Append(']').ToString();}
    static string Clips(AnimatorClipInfo[] clips,string context){StringBuilder b=new StringBuilder("[");for(int i=0;i<clips.Length;i++){if(i!=0)b.Append(',');if(clips[i].clip==null)throw new InvalidOperationException("Null clip: "+context+"["+i+"]");b.Append("{\"name\":").Append(Q(clips[i].clip.name)).Append(",\"weight\":").Append(N(clips[i].weight,context+"["+i+"]weight")).Append(",\"length\":").Append(N(clips[i].clip.length,context+"["+i+"]length")).Append('}');}return b.Append(']').ToString();}
    void WritePose(){
        clone.AssertPure();Animator a=clone.animator;StringBuilder b=new StringBuilder("{\"type\":\"frame\",\"sample\":");
        b.Append(index).Append(",\"animatorSpeed\":").Append(N(a.speed,"animator.speed")).Append(",\"sourceSample\":").Append(ShieldReplayPlan.Samples[index]).Append(",\"frame\":").Append(Time.frameCount)
         .Append(",\"requestedBaseTime\":").Append(N(ShieldReplayPlan.BaseTimes[index],"requestedBaseTime")).Append(",\"requestedBlockTime\":").Append(N(ShieldReplayPlan.BlockTimes[index],"requestedBlockTime"))
         .Append(",\"layers\":[");
        for(int i=0;i<a.layerCount;i++){
            if(i!=0)b.Append(',');AnimatorStateInfo s=a.GetCurrentAnimatorStateInfo(i),next=a.GetNextAnimatorStateInfo(i);
            string context="layer["+i+"]"+a.GetLayerName(i)+" state="+s.fullPathHash;
            float weight=a.GetLayerWeight(i);bool transition=a.IsInTransition(i);
            AnimatorClipInfo[] currentClips=a.GetCurrentAnimatorClipInfo(i),nextClips=a.GetNextAnimatorClipInfo(i);
            string currentReason,nextReason,lengthReason;
            string currentTime=ShieldCaptureNumbers.CurrentClock(s.normalizedTime,i,transition,weight,currentClips.Length,context+".normalizedTime",out currentReason);
            string nextTime=ShieldCaptureNumbers.NextClock(next.normalizedTime,transition,context+".nextNormalizedTime hash="+next.fullPathHash,out nextReason);
            string stateLength=ShieldCaptureNumbers.StateLength(s.length,a.speed,context+".length",out lengthReason);
            b.Append("{\"name\":").Append(Q(a.GetLayerName(i))).Append(",\"weight\":").Append(N(weight,context+".weight"))
             .Append(",\"inTransition\":").Append(transition?"true":"false")
             .Append(",\"state\":").Append(s.fullPathHash).Append(",\"normalizedTime\":").Append(currentTime)
             .Append(",\"normalizedTimeAbsentReason\":").Append(Q(currentReason)).Append(",\"length\":").Append(stateLength)
             .Append(",\"lengthAbsentReason\":").Append(Q(lengthReason)).Append(",\"stateSpeed\":").Append(N(s.speed,context+".speed"))
             .Append(",\"stateSpeedMultiplier\":").Append(N(s.speedMultiplier,context+".speedMultiplier"))
             .Append(",\"nextState\":").Append(next.fullPathHash).Append(",\"nextNormalizedTime\":").Append(nextTime)
             .Append(",\"nextTimeAbsentReason\":").Append(Q(nextReason))
             .Append(",\"clips\":").Append(Clips(currentClips,context+".clips")).Append(",\"nextClips\":").Append(Clips(nextClips,context+".nextClips")).Append('}');
        }
        b.Append("],\"parameters\":{");bool first=true;
        foreach(AnimatorControllerParameter p in a.parameters){
            if(p.type==AnimatorControllerParameterType.Trigger)continue;if(!first)b.Append(',');first=false;b.Append(Q(p.name)).Append(':');
            if(p.type==AnimatorControllerParameterType.Bool)b.Append(a.GetBool(p.name)?"true":"false");
            else if(p.type==AnimatorControllerParameterType.Int)b.Append(a.GetInteger(p.name));else b.Append(N(a.GetFloat(p.name),"parameter."+p.name));
        }
        b.Append("},\"bones\":[");Matrix4x4 inverse=clone.root.transform.worldToLocalMatrix;
        for(int i=0;i<clone.bones.Length;i++){
            if(i!=0)b.Append(',');Transform t=clone.bones[i];b.Append("{\"path\":").Append(Q(clone.paths[i]))
             .Append(",\"local\":").Append(Matrix(Matrix4x4.TRS(t.localPosition,t.localRotation,t.localScale),"bone."+clone.paths[i]+".local"))
             .Append(",\"model\":").Append(Matrix(inverse*t.localToWorldMatrix,"bone."+clone.paths[i]+".model")).Append('}');
        }
        Write(b.Append("]}").ToString());
    }
    void Write(string row){long count=Encoding.UTF8.GetByteCount(row)+1;if(bytes+count>4L*1024*1024)throw new InvalidOperationException("Output byte limit");output.WriteLine(row);output.Flush();bytes+=count;}
    void Finish(string reason,string error){
        if(finished)return;finished=true;if(clone!=null)clone.Release();
        if(output!=null){try{Write("{\"type\":\"end\",\"reason\":"+Q(reason)+",\"frames\":"+frames+",\"error\":"+Q(error)+"}");}finally{output.Dispose();output=null;}}
    }
    void OnApplicationQuit(){Finish("application-quit",null);}
}
}
