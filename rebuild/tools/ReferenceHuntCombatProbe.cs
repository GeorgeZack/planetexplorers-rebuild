// Observation-only: existing mission agents, no spawn/RNG/input/save calls.
using System;
using System.IO;
using System.Text;
using System.Collections;
using System.Globalization;
using System.Reflection;
using UnityEngine;
using Pathea;
using SkillSystem;
using System.Collections.Generic;
namespace PeReference {
public sealed class PoseProbe : MonoBehaviour {
    static PoseProbe instance;
    StreamWriter output;
    string arm, stop;
    int frames, maxFrames;
    long bytes;
    float installed, started = -1, waitSeconds, seconds;
    bool finished, captureAnimators, traceStages;
    void Trace(string stage) { if (traceStages) { Console.Error.WriteLine("[hunt-combat-stage] sample=" + frames + " frame=" + Time.frameCount + " " + stage); Console.Error.Flush(); } }
    FieldInfo agentsField, poolInstance, poolList;
    readonly Dictionary<SkInst, int> skillIds = new Dictionary<SkInst, int>();
    static FieldInfo Field(Type type, string name, bool isStatic) {
        for (; type != null; type = type.BaseType) {
            FieldInfo f = type.GetField(name, BindingFlags.Public | BindingFlags.NonPublic |
                BindingFlags.DeclaredOnly | (isStatic ? BindingFlags.Static : BindingFlags.Instance));
            if (f != null) return f;
        }
        throw new InvalidOperationException("Required field unavailable: " + name);
    }
    static object Read(object value, string field) { return value == null ? null : Field(value.GetType(), field, false).GetValue(value); }
    static string B(bool v) { return v ? "true" : "false"; }
    static string Scalar(object v) { return v == null ? "null" : v is bool ? B((bool)v) : Convert.ToString(v, C); }

    const long MaxBytes = 64L * 1024 * 1024;
    static readonly CultureInfo C = CultureInfo.InvariantCulture;
    public static void Install() {
        if (instance != null) return;
        GameObject host = new GameObject("ReferenceHuntCombatProbe (observation only)");
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
        captureAnimators = Setting("PE_HUNT_COMBAT_ANIMATORS", 1, 0, 1) != 0;
        traceStages = Setting("PE_HUNT_COMBAT_TRACE", 0, 0, 1) != 0;
        maxFrames = Setting("PE_POSE_FRAMES", 600, 1, 3600);
        seconds = Setting("PE_POSE_SECONDS", 20, 1, 120);
        waitSeconds = Setting("PE_POSE_WAIT", 900, 1, 1800);
        Type pool = typeof(SkInst).Assembly.GetType("SkillSystem.SkInstPool", true);
        poolInstance = Field(pool, "instance", true); poolList = Field(pool, "_skInsts", false);
        // Verify installed reflection contracts without invoking lazy getters.
        foreach (string name in new string[] {"_bExecExtEnable", "_startTime", "_coolStat", "_hitInGuide"}) Field(typeof(SkInst), name, false);
        foreach (string name in new string[] {"m_Visions", "m_Hears", "m_FrameCount", "m_CanAttack", "m_CanSearch", "m_Enemy", "m_Escape"}) Field(typeof(TargetCmpt), name, false);
        agentsField = typeof(SceneEntityMissionPointMonsterKill).GetField("_agents", BindingFlags.NonPublic | BindingFlags.Instance);
        if (agentsField == null) throw new InvalidOperationException("Source mission agent list unavailable");
        output = new StreamWriter(new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read), new UTF8Encoding(false));
        installed = Time.realtimeSinceStartup;
        Write("{\"type\":\"header\",\"schema\":1,\"kind\":\"observed-original-hunt-combat\",\"mission\":9126,\"target\":1518,\"phase\":\"WaitForEndOfFrame\",\"unity\":" + Q(Application.unityVersion) +
              ",\"probeSourceSha256\":" + Q(Environment.GetEnvironmentVariable("PE_HUNT_COMBAT_PROBE_SHA256")) + ",\"captureAnimators\":" + B(captureAnimators) + ",\"traceStages\":" + B(traceStages) + ",\"maxFrames\":" + maxFrames + ",\"maxBytes\":" + MaxBytes + "}");
        output.Flush();
        Console.Error.WriteLine("[hunt-combat] Waiting for arm file: " + arm);
        StartCoroutine(Capture());
    }
    IEnumerator Capture() {
        while (!finished) {
            yield return new WaitForEndOfFrame();
            try { Tick(); }
            catch (Exception error) { Console.Error.WriteLine("[hunt-combat] Failed: " + error); Finish("error"); }
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
        if (frames >= maxFrames || bytes >= MaxBytes - 65536) { Finish("capture-limit"); return; }
        Trace("sample-begin");
        // Existing fields/read-only lookup only: no lazy singleton construction.
        MainPlayerCmpt main = MainPlayerCmpt.gMainPlayer;
        PeEntity player = main == null ? null : main.GetComponent<PeEntity>();
        SceneEntityCreator creator = SceneEntityCreator.self;
        SceneEntityMissionPointMonsterKill point = creator == null ? null : creator.GetMissionPoint(9126, 1518) as SceneEntityMissionPointMonsterKill;
        IList agents = point == null ? null : agentsField.GetValue(point) as IList;
        if (point != null && agents == null) throw new InvalidOperationException("Unexpected mission list type");
        if (agents != null && agents.Count > 64) throw new InvalidOperationException("Mission list exceeds observation bound");
        MissionManager manager = MissionManager.Instance;
        PlayerMission missions = manager == null ? null : manager.m_PlayerMission;
        bool active = missions != null && missions.HasMission(9126);
        string counter = active ? missions.GetQuestVariable(9126, "MONSTER0") : null;
        StringBuilder b = new StringBuilder(8192);
        b.Append("{\"type\":\"frame\",\"sample\":").Append(frames).Append(",\"frame\":").Append(Time.frameCount)
         .Append(",\"time\":").Append(F(Time.time)).Append(",\"realtime\":").Append(F(now))
         .Append(",\"missionActive\":").Append(active ? "true" : "false")
         .Append(",\"counter\":").Append(Q(counter)).Append(",\"missionPointPresent\":").Append(point == null ? "false" : "true")
         .Append(",\"player\":").Append(Entity(player)).Append(",\"agents\":[");
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
        b.Append("]}"); Write(b.ToString()); output.Flush(); Trace("sample-written"); frames++;
    }
    string Entity(PeEntity e) {
        if (e == null) return "null";
        Trace("entity " + e.Id);
        PeTrans t = e.peTrans;
        return "{\"id\":" + e.Id + ",\"prototype\":" + e.ProtoID + ",\"prototypeType\":" + (int)e.proto +
            ",\"position\":" + V(e.position) + ",\"rotation\":" + R(e.rotation) +
            ",\"scale\":" + (t == null ? "null" : V(t.scale)) +
            ",\"hp\":" + F(e.GetAttribute(AttribType.Hp)) + ",\"maxHp\":" + F(e.GetAttribute(AttribType.HpMax)) +
            ",\"attack\":" + F(e.GetAttribute(AttribType.Atk)) + ",\"defense\":" + F(e.GetAttribute(AttribType.Def)) +
            ",\"campId\":" + F(e.GetAttribute(AttribType.CampID)) + ",\"damageId\":" + F(e.GetAttribute(AttribType.DamageID)) +
            ",\"defaultPlayerId\":" + F(e.GetAttribute(AttribType.DefaultPlayerID)) + ",\"combat\":" + Combat(e) + "}";
    }
    static string PathOf(Transform t) {
        string s = t.name; int n = 0;
        while (t.parent != null && ++n < 64) { t = t.parent; s = t.name + "/" + s; }
        return s;
    }
    static string EnemyId(object value) {
        Enemy e = value as Enemy;
        return e == null || e.entityTarget == null ? "null" : e.entityTarget.Id.ToString(C);
    }
    static string Sensor(Component c) {
        if (c == null) return "null";
        PEVision v = c as PEVision; PEHearing h = c as PEHearing;
        if (v == null && h == null) throw new InvalidOperationException("Unexpected perception class");
        Transform t = c.transform;
        IList entities = Read(c, "m_Entities") as IList;
        if (entities != null && entities.Count > 512) throw new InvalidOperationException("Sensor entity bound");
        StringBuilder b = new StringBuilder("{\"kind\":").Append(Q(v == null ? "hearing" : "vision"))
          .Append(",\"id\":").Append(c.GetInstanceID()).Append(",\"path\":").Append(Q(PathOf(t)))
          .Append(",\"enabled\":").Append(B(((Behaviour)c).enabled)).Append(",\"active\":").Append(B(c.gameObject.activeInHierarchy))
          .Append(",\"radius\":").Append(F(v == null ? h.radius : v.radius)).Append(",\"position\":").Append(V(t.position))
          .Append(",\"rotation\":").Append(R(t.rotation)).Append(",\"localPosition\":").Append(V(t.localPosition))
          .Append(",\"lossyScale\":").Append(V(t.lossyScale)).Append(",\"blockLayer\":").Append(Scalar(Read(c,"blockLayer")))
          .Append(",\"componentFrame\":").Append(Scalar(Read(c,"m_FrameCount")));
        if (v != null) b.Append(",\"angle\":").Append(F(v.angle)).Append(",\"axis\":").Append(V(v.axis)).Append(",\"worldAxis\":").Append(V(t.TransformDirection(v.axis)));
        else b.Append(",\"ignoreBlock\":").Append(B(h.isIgnoreBlock));
        b.Append(",\"entities\":[");
        if (entities != null) for (int i=0; i<entities.Count; i++) { if(i>0)b.Append(','); PeEntity e=entities[i] as PeEntity; b.Append(e == null ? "null" : e.Id.ToString(C)); }
        return b.Append("]}").ToString();
    }
    string Combat(PeEntity e) {
        StringBuilder b = new StringBuilder("{\"hasView\":").Append(B(e.hasView)).Append(",\"center\":").Append(V(e.centerPos))
          .Append(",\"maxRadius\":").Append(F(e.maxRadius)).Append(",\"target\":");
        Trace("target/sensors " + e.Id);
        TargetCmpt target = e.target;
        if(target == null) b.Append("null");
        else {
            b.Append("{\"componentFrame\":").Append(Scalar(Read(target,"m_FrameCount")))
              .Append(",\"canAttackFlag\":").Append(Scalar(Read(target,"m_CanAttack"))).Append(",\"canSearchFlag\":").Append(Scalar(Read(target,"m_CanSearch")))
              .Append(",\"rawSelectedEntity\":").Append(EnemyId(Read(target,"m_Enemy"))).Append(",\"escapeEntity\":").Append(EnemyId(Read(target,"m_Escape")))
              .Append(",\"selectedEntity\":").Append(EnemyId(target.GetAttackEnemy())).Append(",\"sensors\":[");
            bool first = true;
            foreach(string name in new string[]{"m_Visions","m_Hears"}) {
                IList sensors = Read(target,name) as IList;
                if(sensors != null && sensors.Count>16) throw new InvalidOperationException("Sensor count bound");
                if(sensors != null) foreach(object sensor in sensors) { if(!first)b.Append(','); first=false; b.Append(Sensor(sensor as Component)); }
            }
            b.Append("]}");
        }
        b.Append(",\"skills\":[");
        Trace("skills " + e.Id);
        object pool = poolInstance.GetValue(null); IList skills = pool == null ? null : poolList.GetValue(pool) as IList;
        if(skills != null && skills.Count>4096) throw new InvalidOperationException("Skill pool bound");
        int count=0;
        if(skills != null) foreach(object value in skills) {
            SkInst sk = value as SkInst;
            if(sk == null || sk.Caster != e.skEntity) continue;
            if(++count>64) throw new InvalidOperationException("Entity skill bound");
            int id; if(!skillIds.TryGetValue(sk,out id)) { if(skillIds.Count>=4096)throw new InvalidOperationException("Skill identity bound"); id=skillIds.Count+1; skillIds.Add(sk,id); }
            if(count>1)b.Append(',');
            PeEntity tar = sk.Target == null ? null : sk.Target.GetComponent<PeEntity>();
            IList hits=Read(sk,"_hitInGuide") as IList;
            b.Append("{\"instance\":").Append(id).Append(",\"skill\":").Append(sk.SkillID).Append(",\"guide\":").Append(sk.GuideCnt)
              .Append(",\"active\":").Append(B(sk.IsActive)).Append(",\"execExtEnable\":").Append(Scalar(Read(sk,"_bExecExtEnable")))
              .Append(",\"startTime\":").Append(F((float)Read(sk,"_startTime"))).Append(",\"coolStat\":").Append(Scalar(Read(sk,"_coolStat")))
              .Append(",\"targetEntity\":").Append(tar == null ? "null" : tar.Id.ToString(C)).Append(",\"hitCount\":").Append(hits == null ? 0 : hits.Count).Append('}');
        }
        b.Append("],\"animators\":[");
        Trace("animators " + e.Id);
        BiologyViewCmpt view=captureAnimators && e.hasView ? e.GetComponent<BiologyViewCmpt>() : null; Transform model=view == null ? null : view.modelTrans;
        Animator[] animators=model == null ? new Animator[0] : model.GetComponentsInChildren<Animator>(true);
        if(animators.Length>8)throw new InvalidOperationException("Animator bound");
        for(int i=0;i<animators.Length;i++) {
            if(i>0)b.Append(','); Animator a=animators[i];
            b.Append("{\"id\":").Append(a.GetInstanceID()).Append(",\"path\":").Append(Q(PathOf(a.transform)))
              .Append(",\"enabled\":").Append(B(a.enabled)).Append(",\"active\":").Append(B(a.gameObject.activeInHierarchy))
              .Append(",\"speed\":").Append(F(a.speed)).Append(",\"controller\":").Append(Q(a.runtimeAnimatorController == null ? null : a.runtimeAnimatorController.name)).Append(",\"layers\":[");
            if(a.layerCount>32)throw new InvalidOperationException("Animator layer bound");
            for(int j=0;j<a.layerCount;j++) {
                if(j>0)b.Append(','); b.Append("{\"name\":").Append(Q(a.GetLayerName(j))).Append(",\"weight\":").Append(F(a.GetLayerWeight(j)))
                  .Append(",\"transition\":").Append(B(a.IsInTransition(j))).Append(",\"current\":"); State(b,a.GetCurrentAnimatorStateInfo(j));
                b.Append(",\"next\":"); State(b,a.GetNextAnimatorStateInfo(j));
                b.Append(",\"clips\":"); Clips(b,a.GetCurrentAnimatorClipInfo(j)); b.Append('}');
            }
            b.Append("]}");
        }
        return b.Append("]}").ToString();
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
    void Write(string line) { int n = Encoding.UTF8.GetByteCount(line) + 1; if (bytes + n > MaxBytes) throw new InvalidOperationException("Capture byte bound"); output.WriteLine(line); bytes += n; }
    void Finish(string reason) {
        if (finished) return; finished = true;
        if (output != null) { Write("{\"type\":\"end\",\"reason\":" + Q(reason) + ",\"frames\":" + frames + "}"); output.Dispose(); output = null; }
        Console.Error.WriteLine("[hunt-combat] Finished: " + reason);
    }
    void OnDestroy() { Finish("destroy"); }
}
}
