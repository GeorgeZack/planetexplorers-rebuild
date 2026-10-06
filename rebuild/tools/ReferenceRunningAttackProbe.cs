// Launch-only observer: no player setters, inputs, skill calls, cloning or saves.
using System;
using System.IO;
using System.Text;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using UnityEngine;
using Pathea;
using SkillSystem;
namespace PeReference {
public sealed class PoseProbe : MonoBehaviour {
    static PoseProbe instance;
    static readonly CultureInfo C = CultureInfo.InvariantCulture;
    const long MaxBytes = 32L * 1024 * 1024;
    StreamWriter output;
    string arm, stop;
    int samples, frames, maxFrames, skillSequence;
    long bytes;
    float installed, started = -1, seconds, waitSeconds;
    bool finished;
    readonly Dictionary<SkInst,int> skillIds = new Dictionary<SkInst,int>();
    static FieldInfo Field(Type type, string name) {
        for (; type != null; type = type.BaseType) {
            FieldInfo f=type.GetField(name, BindingFlags.Public|BindingFlags.NonPublic|BindingFlags.Instance|BindingFlags.DeclaredOnly);
            if (f != null) return f;
        }
        throw new InvalidOperationException("Required installed field missing: " + name);
    }
    static object Read(object value,string field) { return value == null ? null : Field(value.GetType(),field).GetValue(value); }
    static string B(bool v) { return v ? "true" : "false"; }
    static int Setting(string name,int fallback,int lo,int hi) {
        string s=Environment.GetEnvironmentVariable(name); int n;
        if(String.IsNullOrEmpty(s)) return fallback;
        if(!Int32.TryParse(s,out n)||n<lo||n>hi)throw new ArgumentException(name);
        return n;
    }
    public static void Install() {
        if(instance != null)return;
        GameObject host=new GameObject("ReferenceRunningAttackProbe (observation only)");
        host.hideFlags=HideFlags.HideAndDontSave; DontDestroyOnLoad(host);
        instance=host.AddComponent<PoseProbe>();
        try { instance.Configure(); } catch { Destroy(host);throw; }
    }
    void Configure() {
        string path=Environment.GetEnvironmentVariable("PE_POSE_OUTPUT");
        arm=Environment.GetEnvironmentVariable("PE_POSE_ARM");stop=Environment.GetEnvironmentVariable("PE_POSE_STOP");
        if(String.IsNullOrEmpty(path)||!Path.IsPathRooted(path)||String.IsNullOrEmpty(arm)||String.IsNullOrEmpty(stop))
            throw new ArgumentException("Fresh absolute output and arm/stop required");
        maxFrames=Setting("PE_POSE_FRAMES",7200,1,7200);
        seconds=Setting("PE_POSE_SECONDS",120,1,120);waitSeconds=Setting("PE_POSE_WAIT",900,1,1800);
        Field(typeof(Motion_Equip),"m_SwordAttack"); Field(typeof(AnimatorCmpt),"m_Animator");
        foreach(string name in new string[]{"m_SkillInst","m_AttackInAir","m_AttackInWater","m_AttackInMove","m_AttackInSprint","m_CombTime","m_EndAction","m_AttackDir","m_AttackTargetDir"}) Field(typeof(Action_SwordAttack),name);
        foreach(string name in new string[]{"_bExecExtEnable","_startTime","_coolStat"}) Field(typeof(SkInst),name);
        output=new StreamWriter(new FileStream(path,FileMode.CreateNew,FileAccess.Write,FileShare.Read),new UTF8Encoding(false));
        installed=Time.realtimeSinceStartup;
        Write("{\"type\":\"header\",\"schema\":1,\"kind\":\"observed-original-running-attack\",\"phase\":\"WaitForEndOfFrame\",\"unity\":"+Q(Application.unityVersion)+",\"probeSourceSha256\":"+Q(Environment.GetEnvironmentVariable("PE_RUNNING_PROBE_SHA256"))+",\"maxFrames\":"+maxFrames+",\"maxSeconds\":"+F(seconds)+",\"maxBytes\":"+MaxBytes+"}");
        output.Flush();Console.Error.WriteLine("[running-attack] Waiting for arm: "+arm);StartCoroutine(Capture());
    }
    IEnumerator Capture() {
        while(!finished) { yield return new WaitForEndOfFrame(); try { Tick(); } catch(Exception e) { Console.Error.WriteLine("[running-attack] "+e);Finish("error"); } }
        Destroy(gameObject);
    }
    void Tick() {
        float now=Time.realtimeSinceStartup;
        if(File.Exists(stop)){Finish("stop-file");return;}
        if(started<0&&now-installed>=waitSeconds){Finish("wait-limit");return;}
        if(!File.Exists(arm))return;
        if(started<0)started=now;
        if(now-started>=seconds){Finish("time-limit");return;}
        if(frames>=maxFrames||bytes>=MaxBytes-131072){Finish("capture-limit");return;}
        frames++;
        MainPlayerCmpt main=MainPlayerCmpt.gMainPlayer;
        PeEntity e=main==null?null:main.GetComponent<PeEntity>();
        Motion_Equip equip=e==null?null:e.GetComponent<Motion_Equip>();
        Action_SwordAttack action=Read(equip,"m_SwordAttack") as Action_SwordAttack;
        Motion_Move move=e==null?null:e.GetComponent<Motion_Move>();
        AnimatorCmpt anim=e==null?null:e.GetComponent<AnimatorCmpt>();
        Animator a=Read(anim,"m_Animator") as Animator;
        BiologyViewCmpt view=e==null||!e.hasView?null:e.GetComponent<BiologyViewCmpt>();
        Transform model=view==null?null:view.modelTrans;
        // Never query native Animator state during loading or for an inactive model.
        if(e==null||action==null||move==null||anim==null||model==null||a==null||!a.enabled||!a.gameObject.activeInHierarchy||a.runtimeAnimatorController==null||!a.isInitialized)return;
        int layers=a.layerCount;if(layers<1)return;if(layers>32)throw new InvalidOperationException("Animator layer bound");
        SkInst sk=Read(action,"m_SkillInst") as SkInst;int sid=0;
        if(sk!=null&&!skillIds.TryGetValue(sk,out sid)) { if(skillIds.Count>=1024)throw new InvalidOperationException("Skill identity bound");sid=++skillSequence;skillIds.Add(sk,sid); }
        StringBuilder b=new StringBuilder(4096);
        b.Append("{\"type\":\"frame\",\"sample\":").Append(samples).Append(",\"frame\":").Append(Time.frameCount)
         .Append(",\"time\":").Append(F(Time.time)).Append(",\"realtime\":").Append(F(now)).Append(",\"dt\":").Append(F(Time.deltaTime)).Append(",\"fixedDt\":").Append(F(Time.fixedDeltaTime))
         .Append(",\"playerId\":").Append(e.Id).Append(",\"position\":").Append(V(e.position)).Append(",\"rotation\":").Append(R(e.rotation))
         .Append(",\"modelPosition\":").Append(V(model.position)).Append(",\"modelRotation\":").Append(R(model.rotation))
         .Append(",\"velocity\":").Append(V(move.velocity)).Append(",\"grounded\":").Append(B(move.grounded))
         .Append(",\"motorVelocity\":").Append(action.phyMotor==null?"null":V(action.phyMotor.velocity))
         .Append(",\"lastMove\":").Append(V(anim.m_LastMove)).Append(",\"lastRotation\":").Append(R(anim.m_LastRot))
         .Append(",\"stamina\":").Append(F(e.GetAttribute(AttribType.Stamina)))
         .Append(",\"action\":{\"moveThreshold\":").Append(F(action.m_MoveAttackSpeed)).Append(",\"sprintThreshold\":").Append(F(action.m_SprintAttackSpeed))
         .Append(",\"weaponPrototype\":").Append(action.sword==null||action.sword.m_ItemObj==null?"null":action.sword.m_ItemObj.protoId.ToString(C))
         .Append(",\"firstPerson\":").Append(B(action.firstPersonAttack)).Append(",\"useStamina\":").Append(B(action.m_UseStamina));
        foreach(string name in new string[]{"m_AttackInAir","m_AttackInWater","m_AttackInMove","m_AttackInSprint","m_EndAction"}) b.Append(',').Append(Q(name)).Append(':').Append(B((bool)Read(action,name)));
        b.Append(",\"combo\":").Append((int)Read(action,"m_CombTime"))
         .Append(",\"direction\":").Append(V((Vector3)Read(action,"m_AttackDir"))).Append(",\"targetDirection\":").Append(V((Vector3)Read(action,"m_AttackTargetDir")))
         .Append(",\"skill\":");
        if(sk==null)b.Append("null");
        else b.Append("{\"instance\":").Append(sid).Append(",\"id\":").Append(sk.SkillID).Append(",\"guide\":").Append(sk.GuideCnt)
          .Append(",\"active\":").Append(B(sk.IsActive)).Append(",\"execExtEnable\":").Append(B((bool)Read(sk,"_bExecExtEnable")))
          .Append(",\"startTime\":").Append(F((float)Read(sk,"_startTime"))).Append(",\"coolStat\":").Append(Convert.ToInt32(Read(sk,"_coolStat"))).Append('}');
        b.Append("},\"animator\":{\"id\":").Append(a.GetInstanceID()).Append(",\"controller\":").Append(Q(a.runtimeAnimatorController.name))
         .Append(",\"avatar\":").Append(Q(a.avatar==null?null:a.avatar.name))
         .Append(",\"speed\":").Append(F(a.speed)).Append(",\"deltaPosition\":").Append(V(a.deltaPosition)).Append(",\"deltaRotation\":").Append(R(a.deltaRotation))
         .Append(",\"rootPosition\":").Append(V(a.rootPosition)).Append(",\"rootRotation\":").Append(R(a.rootRotation))
         .Append(",\"applyRootMotion\":").Append(B(a.applyRootMotion)).Append(",\"layers\":[");
        for(int i=0;i<layers;i++) {
            if(i>0)b.Append(',');b.Append("{\"name\":").Append(Q(a.GetLayerName(i))).Append(",\"weight\":").Append(F(a.GetLayerWeight(i)))
             .Append(",\"transition\":").Append(B(a.IsInTransition(i))).Append(",\"current\":");State(b,a.GetCurrentAnimatorStateInfo(i));
            b.Append(",\"next\":");State(b,a.GetNextAnimatorStateInfo(i));b.Append(",\"clips\":");Clips(b,a.GetCurrentAnimatorClipInfo(i));
            b.Append(",\"nextClips\":");Clips(b,a.GetNextAnimatorClipInfo(i));b.Append('}');
        }
        b.Append("]}}");Write(b.ToString());output.Flush();samples++;
    }
    static void State(StringBuilder b, AnimatorStateInfo s) {
        b.Append("{\"fullPathHash\":").Append(s.fullPathHash).Append(",\"shortNameHash\":").Append(s.shortNameHash).Append(",\"normalizedTime\":").Append(F(s.normalizedTime));
        b.Append(",\"length\":").Append(F(s.length)).Append(",\"speed\":").Append(F(s.speed)).Append(",\"speedMultiplier\":").Append(F(s.speedMultiplier)).Append(",\"loop\":").Append(s.loop ? "true}" : "false}");
    }
    static void Clips(StringBuilder b, AnimatorClipInfo[] clips) {
        if (clips.Length > 64) throw new InvalidOperationException("Clip bound exceeded");
        b.Append('['); for (int i = 0; i < clips.Length; i++) { if (i > 0) b.Append(','); AnimationClip clip = clips[i].clip;
            b.Append("{\"name\":").Append(Q(clip == null ? null : clip.name)).Append(",\"id\":").Append(clip == null ? 0 : clip.GetInstanceID()).Append(",\"weight\":").Append(F(clips[i].weight)).Append(",\"length\":").Append(clip == null ? "null" : F(clip.length)).Append('}'); }
        b.Append(']');
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

    void Write(string line) { int n=Encoding.UTF8.GetByteCount(line)+1;if(n>131072||bytes+n>MaxBytes)throw new InvalidOperationException("Capture size bound");output.WriteLine(line);bytes+=n; }
    void Finish(string reason) {
        if(finished)return;finished=true;
        if(output!=null){Write("{\"type\":\"end\",\"reason\":"+Q(reason)+",\"frames\":"+samples+",\"observedLoops\":"+frames+"}");output.Dispose();output=null;}
        Console.Error.WriteLine("[running-attack] Finished: "+reason+" samples="+samples);
    }
    void OnDestroy(){Finish("destroy");}
}}
