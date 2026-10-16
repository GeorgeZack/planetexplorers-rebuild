// Observation-only runtime probe. Built against the installed Unity 5.2 assemblies.
// No Animator setters, animation playback, input, gameplay calls or save APIs.
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
    int maxFrames, frames;
    float installedAt, startedAt = -1, waitSeconds, captureSeconds;
    long bytes;
    bool finished;
    FieldInfo playerField;
    AnimatorCmpt observedAnimation;
    MotionMgrCmpt observedMotion;
    Animator observedAnimator;
    FieldInfo eventField, runningField;
    int eventRecords;
    const int MaxEvents = 4096;
    // Opt-in harvest outcomes (PE_GATHER_OUTCOMES=1). Raw fields only, plus the
    // audited pure GlobalTreeInfo.WorldPos arithmetic. Never call
    // SkEntitySubTerrain.Instance, GetTreeHP, PlayerPackageCmpt.package or RNG.
    const int MaxBagItems = 1024, BagTabs = 4, MaxSlotsPerTab = 1024;
    bool outcomes, postAttached;
    int openGather; // sequence of the Gather receipt whose post snapshot is pending; 0 when none
    FieldInfo subTerrainField, treeHpField, packageField, playerPakField, slotArrayField, slotsField, grantField;
    PeEvent.Event<PlayerPackageCmpt.GetItemEventArg> grantEvent;
    static FieldInfo PrivateField(Type type, string name) {
        for (; type != null; type = type.BaseType) {
            FieldInfo f = type.GetField(name, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.DeclaredOnly);
            if (f != null) return f;
        }
        throw new InvalidOperationException("Required installed field missing: " + name);
    }
    static object Raw(object value, string name) {
        return value == null ? null : PrivateField(value.GetType(), name).GetValue(value);
    }
    void ObservePlayer(Component player, Transform model) {
        AnimatorCmpt animation = player.GetComponent<AnimatorCmpt>();
        MotionMgrCmpt motion = player.GetComponent<MotionMgrCmpt>();
        Animator animator = Raw(animation, "m_Animator") as Animator;
        if (animation == null || motion == null || animator == null || !animator.enabled ||
            !animator.gameObject.activeInHierarchy || !animator.isInitialized || animator.runtimeAnimatorController == null)
            throw new InvalidOperationException("Player Animator not ready");
        if (animator.runtimeAnimatorController.name != "FemaleAnimator2" || animator.avatar == null || animator.avatar.name != "FemaleAvatar")
            throw new InvalidOperationException("Gather capture requires the qualified female player rig");
        if (observedAnimation != null) {
            if (observedAnimation != animation || observedMotion != motion || observedAnimator != animator)
                throw new InvalidOperationException("Observed player/model changed; choose a fresh capture");
            return;
        }
        eventField = PrivateField(animation.GetType(), "AnimEvtString");
        runningField = PrivateField(motion.GetType(), "m_RunningAction");
        if (runningField.GetValue(motion) == null) throw new InvalidOperationException("Motion state not ready");
        if (outcomes) PrepareOutcomes(player);
        observedAnimation = animation; observedMotion = motion; observedAnimator = animator;
        animation.AnimEvtString += ObserveAnimation;
        motion.onActionStart += ObserveAdmission;
        motion.onActionEnd += ObserveEnd;
        if (grantEvent != null) grantEvent.Subscribe(ObserveGrant);
        Write("{\"type\":\"subscription\",\"frame\":" + Time.frameCount + ",\"handlers\":" + HandlerOrder() + "}");
        output.Flush();
        Console.Error.WriteLine("[gather] Event and action observers attached; operator may begin");
    }
    string HandlerOrder() {
        Delegate callbacks = eventField.GetValue(observedAnimation) as Delegate;
        Delegate[] handlers = callbacks == null ? new Delegate[0] : callbacks.GetInvocationList();
        if (handlers.Length > 256) throw new InvalidOperationException("Event handler bound exceeded");
        StringBuilder b = new StringBuilder("[");
        for (int i = 0; i < handlers.Length; i++) {
            if (i > 0) b.Append(','); Delegate h = handlers[i];
            b.Append("{\"targetType\":").Append(Q(h.Target == null ? null : h.Target.GetType().FullName));
            b.Append(",\"declaringType\":").Append(Q(h.Method.DeclaringType == null ? null : h.Method.DeclaringType.FullName));
            bool own = System.Object.ReferenceEquals(h.Target,this);
            b.Append(",\"method\":").Append(Q(h.Method.Name)).Append(",\"observer\":").Append(own ? "true" : "false");
            if (outcomes && own) b.Append(",\"role\":").Append(Q(h.Method.Name == "ObserveAnimationPost" ? "post" : "pre"));
            b.Append('}');
        }
        return b.Append(']').ToString();
    }
    void MotionData(StringBuilder b) {
        Array running = runningField.GetValue(observedMotion) as Array;
        if (running == null || running.Length <= 32) throw new InvalidOperationException("Motion array unavailable");
        b.Append("{\"gatherMask\":").Append(observedMotion.GetMaskState((PEActionMask)29) ? "true" : "false");
        b.Append(",\"actions\":[");
        int[] selected = {1,2,3,4,13,16,32};
        for (int i = 0; i < selected.Length; i++) {
            if (i > 0) b.Append(','); int id = selected[i]; object action = running.GetValue(id);
            b.Append("{\"id\":").Append(id).Append(",\"running\":").Append(action == null ? "false" : "true");
            b.Append(",\"waitingRelation\":").Append(action == null ? "null" : ((bool)Raw(action,"m_WaitRelation") ? "true" : "false"));
            b.Append(",\"doActionPending\":").Append(action == null ? "null" : ((bool)Raw(action,"m_DoAction") ? "true" : "false")).Append('}');
        }
        b.Append("]}");
    }
    void ObserveAnimation(string value) {
        Receipt("animation-event", value, -1);
        if (outcomes && value == "Gather" && !finished) {
            // A second Gather before the first post stays visible; the analyzer rejects nesting.
            openGather = eventRecords;
            Outcome("outcome-snapshot", "pre", null);
        }
    }
    void ObserveAnimationPost(string value) {
        if (value != "Gather") return;
        Outcome("outcome-snapshot", "post", null);
        openGather = 0;
    }
    void ObserveGrant(object sender, PlayerPackageCmpt.GetItemEventArg arg) {
        Outcome("item-grant", null, arg == null ? null : "{\"protoId\":" + arg.protoId + ",\"count\":" + arg.count + "}");
    }
    static Delegate Handlers(FieldInfo field, object owner) { return owner == null ? null : field.GetValue(owner) as Delegate; }
    object GatherAction() {
        Delegate callbacks = Handlers(eventField, observedAnimation);
        if (callbacks == null) return null;
        foreach (Delegate h in callbacks.GetInvocationList())
            if (h.Target != null && h.Target.GetType().FullName == "Pathea.Action_Gather" && h.Method.Name == "OnAnimEvent") return h.Target;
        return null;
    }
    void PrepareOutcomes(Component player) {
        PlayerPackageCmpt package = player.GetComponent<PlayerPackageCmpt>();
        if (package == null) throw new InvalidOperationException("Player package component missing");
        subTerrainField = typeof(SkEntitySubTerrain).GetField("m_Instance", BindingFlags.NonPublic | BindingFlags.Static);
        if (subTerrainField == null) throw new InvalidOperationException("Required installed field missing: m_Instance");
        treeHpField = PrivateField(typeof(SkEntitySubTerrain), "treeHPInfos");
        packageField = PrivateField(typeof(PlayerPackageCmpt), "mPackage");
        playerPakField = PrivateField(typeof(PlayerPackage), "_playerPak");
        slotArrayField = PrivateField(typeof(ItemAsset.ItemPackage), "mSlotListArray");
        slotsField = PrivateField(typeof(ItemAsset.SlotList), "mSlots");
        grantField = PrivateField(typeof(PlayerPackageCmpt), "mEventor");
        grantEvent = grantField.GetValue(package) as PeEvent.Event<PlayerPackageCmpt.GetItemEventArg>;
        if (grantEvent == null) throw new InvalidOperationException("Grant event unavailable");
        observedPackage = package;
    }
    PlayerPackageCmpt observedPackage;
    void TryAttachPost() {
        // Only after the original Gather handler exists, so the post handler follows it.
        if (!outcomes || postAttached || GatherAction() == null) return;
        observedAnimation.AnimEvtString += ObserveAnimationPost;
        postAttached = true;
        Outcome("outcome-attach", null, null);
    }
    static void Vector(StringBuilder b, Vector3 v) { b.Append('[').Append(N(v.x)).Append(',').Append(N(v.y)).Append(',').Append(N(v.z)).Append(']'); }
    static GlobalTreeInfo TreeField(object action, string field) {
        return action == null ? null : PrivateField(action.GetType(), field).GetValue(action) as GlobalTreeInfo;
    }
    static void Tree(StringBuilder b, GlobalTreeInfo tree) {
        if (tree == null || tree._treeInfo == null) { b.Append("null"); return; }
        b.Append("{\"terrainIndex\":").Append(tree._terrainIndex).Append(",\"localPosition\":"); Vector(b, tree._treeInfo.m_pos);
        b.Append(",\"prototype\":").Append(tree._treeInfo.m_protoTypeIdx).Append(",\"worldPosition\":"); Vector(b, tree.WorldPos);
        b.Append('}');
    }
    void Herb(StringBuilder b, GlobalTreeInfo tree) {
        // Exact source key only; an absent entry is not HP 255 (that default is GetTreeHP's own insertion).
        object instance = subTerrainField.GetValue(null);
        if (instance == null) { b.Append("{\"instance\":false}"); return; }
        System.Collections.Generic.Dictionary<Vector3, float> hp = treeHpField.GetValue(instance) as System.Collections.Generic.Dictionary<Vector3, float>;
        if (hp == null) throw new InvalidOperationException("Tree HP dictionary unavailable");
        b.Append("{\"instance\":true");
        if (tree != null && tree._treeInfo != null) {
            float value; bool present = hp.TryGetValue(tree.WorldPos, out value);
            b.Append(",\"entry\":").Append(present ? "true" : "false").Append(",\"hp\":").Append(present ? N(value) : "null");
        } else b.Append(",\"entry\":null,\"hp\":null");
        b.Append('}');
    }
    void Bag(StringBuilder b) {
        object package = observedPackage == null ? null : packageField.GetValue(observedPackage);
        if (package == null) { b.Append("null"); return; }
        object pak = playerPakField.GetValue(package);
        Array lists = pak == null ? null : slotArrayField.GetValue(pak) as Array;
        if (lists == null) { b.Append("{\"slotLists\":null}"); return; }
        if (lists.Length != BagTabs) throw new InvalidOperationException("Unexpected bag tab count");
        b.Append("{\"slotLists\":["); int items = 0;
        for (int tab = 0; tab < lists.Length; tab++) {
            if (tab > 0) b.Append(','); object list = lists.GetValue(tab);
            Array slots = list == null ? null : slotsField.GetValue(list) as Array;
            if (slots == null) { b.Append("null"); continue; }
            if (slots.Length > MaxSlotsPerTab) throw new InvalidOperationException("Bag slot bound exceeded");
            b.Append("{\"slots\":").Append(slots.Length).Append(",\"items\":["); bool first = true;
            for (int i = 0; i < slots.Length; i++) {
                object item = slots.GetValue(i); if (item == null) continue;
                if (++items > MaxBagItems) throw new InvalidOperationException("Bag item bound exceeded");
                if (!first) b.Append(','); first = false;
                b.Append("{\"slot\":").Append(i).Append(",\"instanceId\":").Append((int)Raw(item, "mInstanceId"));
                b.Append(",\"protoId\":").Append((int)Raw(item, "mProtoId")).Append(",\"stackCount\":").Append((int)Raw(item, "mStackCount")).Append('}');
            }
            b.Append("]}");
        }
        b.Append("]}");
    }
    void Outcome(string kind, string role, string grant) {
        // Same sequence, byte and failure policy as other receipts; never throws into the game.
        if (finished) return;
        try {
            if (++eventRecords > MaxEvents) throw new InvalidOperationException("Event record bound exceeded");
            StringBuilder b = new StringBuilder("{\"type\":"); b.Append(Q(kind));
            b.Append(",\"sequence\":").Append(eventRecords).Append(",\"frame\":").Append(Time.frameCount);
            b.Append(",\"time\":").Append(N(Time.time)).Append(",\"realtime\":").Append(N(Time.realtimeSinceStartup));
            b.Append(",\"handlers\":").Append(HandlerOrder());
            if (kind != "outcome-attach") b.Append(",\"gatherReceipt\":").Append(openGather == 0 ? "null" : openGather.ToString(Invariant));
            if (role != null) {
                object action = GatherAction(); GlobalTreeInfo selected = TreeField(action, "mOpTreeInfo");
                b.Append(",\"role\":").Append(Q(role)).Append(",\"gatherAction\":").Append(action == null ? "false" : "true");
                b.Append(",\"selected\":"); Tree(b, selected); b.Append(",\"found\":"); Tree(b, TreeField(action, "mFindTreeInfo"));
                b.Append(",\"selectedHp\":"); Herb(b, selected); b.Append(",\"bag\":"); Bag(b);
            }
            if (grant != null) b.Append(",\"grant\":").Append(grant);
            b.Append('}'); Write(b.ToString()); output.Flush();
        } catch (Exception error) { Console.Error.WriteLine("[gather] Outcome observer stopped: " + error.Message); Finish("event-error"); }
    }
    void ObserveAdmission(PEActionType type) { Receipt("action-admission", null, (int)type); }
    void ObserveEnd(PEActionType type) { Receipt("action-end-notification", null, (int)type); }
    void Receipt(string kind, string value, int action) {
        // Never let observer failures escape into the game's multicast dispatch.
        if (finished) return;
        try {
            if (++eventRecords > MaxEvents || (value != null && value.Length > 1024)) throw new InvalidOperationException("Event record bound exceeded");
            StringBuilder b = new StringBuilder("{\"type\":"); b.Append(Q(kind));
            b.Append(",\"sequence\":").Append(eventRecords).Append(",\"frame\":").Append(Time.frameCount);
            b.Append(",\"time\":").Append(N(Time.time)).Append(",\"realtime\":").Append(N(Time.realtimeSinceStartup)).Append(",\"delta\":").Append(N(Time.deltaTime));
            b.Append(",\"event\":").Append(Q(value)).Append(",\"actionId\":").Append(action);
            b.Append(",\"handlers\":").Append(HandlerOrder()).Append(",\"motion\":"); MotionData(b);
            b.Append(",\"animator\":"); AnimatorData(b, observedAnimator); b.Append('}'); Write(b.ToString()); output.Flush();
        } catch (Exception error) { Console.Error.WriteLine("[gather] Observer stopped: " + error.Message); Finish("event-error"); }
    }
    void Unsubscribe() {
        // Remove only our own handlers; preserve every original subscriber.
        try { if (!System.Object.ReferenceEquals(observedAnimation,null)) { observedAnimation.AnimEvtString -= ObserveAnimation; observedAnimation.AnimEvtString -= ObserveAnimationPost; } }
        catch (Exception error) { Console.Error.WriteLine("[gather] Animation unsubscribe: " + error.Message); }
        try { if (!System.Object.ReferenceEquals(observedMotion,null)) observedMotion.onActionStart -= ObserveAdmission; }
        catch (Exception error) { Console.Error.WriteLine("[gather] Admission unsubscribe: " + error.Message); }
        try { if (!System.Object.ReferenceEquals(observedMotion,null)) observedMotion.onActionEnd -= ObserveEnd; }
        catch (Exception error) { Console.Error.WriteLine("[gather] End unsubscribe: " + error.Message); }
        try { if (grantEvent != null) grantEvent.Unsubscribe(ObserveGrant); }
        catch (Exception error) { Console.Error.WriteLine("[gather] Grant unsubscribe: " + error.Message); }
        observedAnimation = null; observedMotion = null; observedAnimator = null; grantEvent = null; observedPackage = null;
    }
    const long MaxBytes = 256L * 1024 * 1024;
    const int MaxTransforms = 1024;
    static readonly CultureInfo Invariant = CultureInfo.InvariantCulture;
    public static void Install() {
        if (instance != null) return;
        string path = Environment.GetEnvironmentVariable("PE_POSE_OUTPUT");
        if (String.IsNullOrEmpty(path) || !Path.IsPathRooted(path)) throw new ArgumentException("Absolute capture path required");
        GameObject host = new GameObject("ReferenceGatherProbe (observation only)");
        host.hideFlags = HideFlags.HideAndDontSave;
        DontDestroyOnLoad(host);
        instance = host.AddComponent<PoseProbe>();
        try { instance.Configure(path); }
        catch { Destroy(host); throw; }
    }
    static int Setting(string name, int fallback, int min, int max) {
        string s = Environment.GetEnvironmentVariable(name); int result;
        if (String.IsNullOrEmpty(s)) return fallback;
        if (!Int32.TryParse(s, out result) || result < min || result > max) throw new ArgumentException(name);
        return result;
    }
    void Configure(string path) {
        arm = Environment.GetEnvironmentVariable("PE_POSE_ARM");
        stop = Environment.GetEnvironmentVariable("PE_POSE_STOP");
        if (String.IsNullOrEmpty(arm) || String.IsNullOrEmpty(stop)) throw new ArgumentException("Arm/stop paths required");
        if (Convert.ToInt32(Enum.Parse(typeof(PEActionType),"Gather")) != 32 || Convert.ToInt32(Enum.Parse(typeof(PEActionMask),"Gather")) != 29) throw new InvalidOperationException("Installed Gather enum identity differs");
        string outcomeSetting = Environment.GetEnvironmentVariable("PE_GATHER_OUTCOMES");
        if (!String.IsNullOrEmpty(outcomeSetting) && outcomeSetting != "1") throw new ArgumentException("PE_GATHER_OUTCOMES");
        outcomes = outcomeSetting == "1";
        maxFrames = Setting("PE_POSE_FRAMES", 600, 1, 3600);
        waitSeconds = Setting("PE_POSE_WAIT", 900, 1, 1800);
        captureSeconds = Setting("PE_POSE_SECONDS", 20, 1, 120);
        foreach (Assembly assembly in AppDomain.CurrentDomain.GetAssemblies()) {
            Type t = assembly.GetType("Pathea.MainPlayerCmpt", false);
            if (t != null) { playerField = t.GetField("gMainPlayer", BindingFlags.Public | BindingFlags.Static); break; }
        }
        if (playerField == null) throw new InvalidOperationException("MainPlayerCmpt.gMainPlayer unavailable");
        output = new StreamWriter(new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read), new UTF8Encoding(false));
        installedAt = Time.realtimeSinceStartup;
        Write("{\"type\":\"header\",\"schema\":1,\"kind\":\"observed-original-gather-pose\",\"callbackPhase\":\"synchronous-event-dispatch\",\"probeSourceSha256\":" + Q(Environment.GetEnvironmentVariable("PE_GATHER_PROBE_SHA256")) + ",\"phase\":\"WaitForEndOfFrame\",\"matrixOrder\":\"row-major\",\"unity\":" + Q(Application.unityVersion) + ",\"maxFrames\":" + maxFrames + ",\"maxBytes\":" + MaxBytes + (outcomes ? ",\"outcomes\":true" : "") + "}");
        WriteHumanTraits();
        output.Flush();
        Console.Error.WriteLine("[pose] Waiting for arm file: " + arm);
        StartCoroutine(Capture());
    }
    IEnumerator Capture() {
        while (!finished) {
            yield return new WaitForEndOfFrame();
            try { Tick(); }
            catch (Exception error) { Console.Error.WriteLine("[pose] Capture error: " + error); Finish("error"); }
        }
        Destroy(gameObject);
    }
    void Tick() {
        float now = Time.realtimeSinceStartup;
        if (File.Exists(stop)) { Finish("stop-file"); return; }
        if (startedAt < 0 && now - installedAt >= waitSeconds) { Finish("arm-or-player-timeout"); return; }
        if (startedAt >= 0 && (frames >= maxFrames || now - startedAt >= captureSeconds)) { Finish("capture-limit"); return; }
        if (!File.Exists(arm)) return;
        Component player = playerField.GetValue(null) as Component;
        if (player == null) return;
        Component view = player.GetComponent("BiologyViewCmpt");
        Transform model = Read(view, "modelTrans") as Transform;
        if (model == null) return;
        ObservePlayer(player, model);
        TryAttachPost();
        // Attachment writes a receipt with its own real-time sample before this frame.
        now = Time.realtimeSinceStartup;
        if (startedAt < 0) { startedAt = now; Console.Error.WriteLine("[pose] Capturing player poses"); }
        Transform[] transforms = model.GetComponentsInChildren<Transform>(true);
        if (transforms.Length > MaxTransforms) throw new InvalidOperationException("Model transform bound exceeded");
        Component common = player.GetComponent("CommonCmpt");
        StringBuilder b = new StringBuilder(65536);
        b.Append("{\"type\":\"frame\",\"frame\":").Append(Time.frameCount).Append(",\"sample\":").Append(frames);
        b.Append(",\"time\":").Append(N(Time.time)).Append(",\"realtime\":").Append(N(now)).Append(",\"delta\":").Append(N(Time.deltaTime)).Append(",\"unscaledDelta\":").Append(N(Time.unscaledDeltaTime));
        b.Append(",\"fixedTime\":").Append(N(Time.fixedTime)).Append(",\"fixedDelta\":").Append(N(Time.fixedDeltaTime));
        b.Append(",\"motion\":"); MotionData(b);
        b.Append(",\"gender\":").Append(Q(Convert.ToString(Read(common, "sex"), Invariant)));
        b.Append(",\"playerRoot\":"); TransformData(b, player.transform);
        b.Append(",\"modelRoot\":"); TransformData(b, model);
        b.Append(",\"animators\":[");
        Animator[] animators = model.GetComponentsInChildren<Animator>(true);
        if (animators.Length > 16) throw new InvalidOperationException("Animator bound exceeded");
        for (int i = 0; i < animators.Length; i++) { if (i > 0) b.Append(','); AnimatorData(b, animators[i]); }
        b.Append("],\"transforms\":[");
        for (int i = 0; i < transforms.Length; i++) { if (i > 0) b.Append(','); TransformData(b, transforms[i]); }
        b.Append("],\"renderers\":[");
        SkinnedMeshRenderer[] renderers = model.GetComponentsInChildren<SkinnedMeshRenderer>(true);
        if (renderers.Length > 64) throw new InvalidOperationException("Renderer bound exceeded");
        for (int i = 0; i < renderers.Length; i++) { if (i > 0) b.Append(','); SkinnedMeshRenderer r = renderers[i];
            b.Append("{\"path\":").Append(Q(PathOf(r.transform))).Append(",\"enabled\":").Append(r.enabled ? "true" : "false");
            b.Append(",\"mesh\":").Append(Q(r.sharedMesh == null ? null : r.sharedMesh.name)).Append(",\"boneIds\":["); Transform[] bones = r.bones;
            if (bones.Length > MaxTransforms) throw new InvalidOperationException("Renderer bone bound exceeded");
            for (int j = 0; j < bones.Length; j++) { if (j > 0) b.Append(','); b.Append(bones[j] == null ? 0 : bones[j].GetInstanceID()); } b.Append("]}"); }
        b.Append("]}"); Write(b.ToString()); frames++; if (frames % 30 == 0) output.Flush();
    }
    static object Read(object value, string name) {
        if (value == null) return null;
        Type type = value.GetType();
        PropertyInfo p = type.GetProperty(name, BindingFlags.Public | BindingFlags.Instance);
        if (p != null) return p.GetValue(value, null);
        FieldInfo f = type.GetField(name, BindingFlags.Public | BindingFlags.Instance);
        return f == null ? null : f.GetValue(value);
    }
    static string Q(string s) {
        if (s == null) return "null";
        StringBuilder b = new StringBuilder("\"");
        foreach (char c in s) { if (c == '"' || c == '\\') b.Append('\\').Append(c); else if (c < 32) b.Append("\\u").Append(((int)c).ToString("x4")); else b.Append(c); }
        return b.Append('"').ToString();
    }
    static string N(float f) { return Single.IsNaN(f) || Single.IsInfinity(f) ? "null" : f.ToString("R", Invariant); }
    static string PathOf(Transform t) {
        string s = t.name; int depth = 0;
        while (t.parent != null && depth++ < 128) { t = t.parent; s = t.name + "/" + s; }
        return s;
    }
    static void Matrix(StringBuilder b, Matrix4x4 m) {
        b.Append('['); for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) { if (r != 0 || c != 0) b.Append(','); b.Append(N(m[r,c])); } b.Append(']');
    }
    static void TransformData(StringBuilder b, Transform t) {
        b.Append("{\"id\":").Append(t.GetInstanceID()).Append(",\"parentId\":").Append(t.parent == null ? 0 : t.parent.GetInstanceID()).Append(",\"path\":").Append(Q(PathOf(t)));
        b.Append(",\"activeInHierarchy\":").Append(t.gameObject.activeInHierarchy ? "true" : "false");
        Vector3 p = t.localPosition, s = t.localScale; Quaternion q = t.localRotation;
        b.Append(",\"localPosition\":[").Append(N(p.x)).Append(',').Append(N(p.y)).Append(',').Append(N(p.z));
        b.Append("],\"localRotation\":[").Append(N(q.x)).Append(',').Append(N(q.y)).Append(',').Append(N(q.z)).Append(',').Append(N(q.w));
        b.Append("],\"localScale\":[").Append(N(s.x)).Append(',').Append(N(s.y)).Append(',').Append(N(s.z)).Append(']');
        b.Append(",\"local\":"); Matrix(b, Matrix4x4.TRS(p, q, s));
        b.Append(",\"world\":"); Matrix(b, t.localToWorldMatrix); b.Append('}');
    }
    static void State(StringBuilder b, AnimatorStateInfo s) {
        b.Append("{\"fullPathHash\":").Append(s.fullPathHash).Append(",\"shortNameHash\":").Append(s.shortNameHash).Append(",\"normalizedTime\":").Append(N(s.normalizedTime));
        b.Append(",\"length\":").Append(N(s.length)).Append(",\"speed\":").Append(N(s.speed)).Append(",\"speedMultiplier\":").Append(N(s.speedMultiplier)).Append(",\"loop\":").Append(s.loop ? "true}" : "false}");
    }
    static void Clips(StringBuilder b, AnimatorClipInfo[] clips) {
        if (clips.Length > 64) throw new InvalidOperationException("Clip bound exceeded");
        b.Append('['); for (int i = 0; i < clips.Length; i++) { if (i > 0) b.Append(','); AnimationClip clip = clips[i].clip;
            b.Append("{\"name\":").Append(Q(clip == null ? null : clip.name)).Append(",\"id\":").Append(clip == null ? 0 : clip.GetInstanceID()).Append(",\"weight\":").Append(N(clips[i].weight)).Append(",\"length\":").Append(clip == null ? "null" : N(clip.length)).Append('}'); }
        b.Append(']');
    }
    void AnimatorData(StringBuilder b, Animator a) {
        b.Append("{\"id\":").Append(a.GetInstanceID()).Append(",\"path\":").Append(Q(PathOf(a.transform))).Append(",\"speed\":").Append(N(a.speed));
        b.Append(",\"avatar\":").Append(Q(a.avatar == null ? null : a.avatar.name)).Append(",\"avatarId\":").Append(a.avatar == null ? 0 : a.avatar.GetInstanceID());
        b.Append(",\"controller\":").Append(Q(a.runtimeAnimatorController == null ? null : a.runtimeAnimatorController.name));
        b.Append(",\"enabled\":").Append(a.enabled ? "true" : "false").Append(",\"applyRootMotion\":").Append(a.applyRootMotion ? "true" : "false");
        b.Append(",\"activeInHierarchy\":").Append(a.gameObject.activeInHierarchy ? "true" : "false");
        b.Append(",\"rootWorld\":"); Matrix(b, a.transform.localToWorldMatrix);
        b.Append(",\"rootInverseWorld\":"); Matrix(b, a.transform.worldToLocalMatrix);
        b.Append(",\"layers\":[");
        if (a.layerCount > 32) throw new InvalidOperationException("Layer bound exceeded");
        for (int i = 0; i < a.layerCount; i++) { if (i > 0) b.Append(',');
            b.Append("{\"name\":").Append(Q(a.GetLayerName(i))).Append(",\"weight\":").Append(N(a.GetLayerWeight(i))).Append(",\"inTransition\":").Append(a.IsInTransition(i) ? "true" : "false");
            b.Append(",\"current\":"); State(b, a.GetCurrentAnimatorStateInfo(i)); b.Append(",\"next\":"); State(b, a.GetNextAnimatorStateInfo(i));
            b.Append(",\"currentClips\":"); Clips(b, a.GetCurrentAnimatorClipInfo(i)); b.Append(",\"nextClips\":"); Clips(b, a.GetNextAnimatorClipInfo(i)); b.Append('}'); }
        b.Append("],\"parameters\":{"); AnimatorControllerParameter[] parameters = a.parameters;
        if (parameters.Length > 512) throw new InvalidOperationException("Parameter bound exceeded");
        for (int i = 0; i < parameters.Length; i++) { if (i > 0) b.Append(','); AnimatorControllerParameter p = parameters[i]; b.Append(Q(p.name)).Append(':');
            if (p.type == AnimatorControllerParameterType.Float) b.Append(N(a.GetFloat(p.nameHash)));
            else if (p.type == AnimatorControllerParameterType.Int) b.Append(a.GetInteger(p.nameHash));
            else if (p.type == AnimatorControllerParameterType.Bool) b.Append(a.GetBool(p.nameHash) ? "true" : "false"); else b.Append("null"); }
        b.Append("},\"humanBones\":{"); bool first = true;
        if (a.isHuman && a.avatar != null && a.avatar.isValid) for (int i = 0; i < (int)HumanBodyBones.LastBone; i++) { Transform t = a.GetBoneTransform((HumanBodyBones)i); if (t == null) continue;
            if (!first) b.Append(','); first = false; b.Append(Q(((HumanBodyBones)i).ToString())).Append(':').Append(t.GetInstanceID()); }
        b.Append("}");
        b.Append('}');
    }
    void WriteHumanTraits() {
        StringBuilder b = new StringBuilder("{\"type\":\"humanTraits\",\"muscles\":[");
        string[] names = HumanTrait.MuscleName;
        for (int i = 0; i < HumanTrait.MuscleCount; i++) { if (i > 0) b.Append(',');
            b.Append("{\"name\":").Append(Q(names[i])).Append(",\"min\":").Append(N(HumanTrait.GetMuscleDefaultMin(i))).Append(",\"max\":").Append(N(HumanTrait.GetMuscleDefaultMax(i))).Append('}'); }
        b.Append("],\"bones\":[");
        for (int i = 0; i < HumanTrait.BoneCount; i++) { if (i > 0) b.Append(','); b.Append("{\"name\":").Append(Q(HumanTrait.BoneName[i])).Append(",\"muscleIndices\":[");
            for (int axis = 0; axis < 3; axis++) { if (axis > 0) b.Append(','); b.Append(HumanTrait.MuscleFromBone(i, axis)); } b.Append("]}"); }
        Write(b.Append("]}").ToString());
    }
    void Write(string s) {
        int size = Encoding.UTF8.GetByteCount(s) + 1;
        if (bytes + size > MaxBytes) throw new InvalidOperationException("Capture byte bound exceeded");
        output.WriteLine(s); bytes += size;
    }
    void Finish(string reason) {
        if (finished) return; finished = true;
        Unsubscribe();
        if (output != null) {
            try { if (bytes < MaxBytes - 1024) Write("{\"type\":\"end\",\"reason\":" + Q(reason) + ",\"frames\":" + frames + ",\"eventRecords\":" + eventRecords + "}"); }
            catch (Exception error) { Console.Error.WriteLine("[pose] Final record unavailable: " + error.Message); }
            finally {
                try { output.Dispose(); }
                catch (Exception error) { Console.Error.WriteLine("[pose] Output close failed: " + error.Message); }
                output = null;
            }
        }
        Console.Error.WriteLine("[pose] Finished: " + reason + "; frames=" + frames);
    }
    void OnApplicationQuit() { Finish("application-quit"); }
    void OnDestroy() { Finish("destroy"); if (instance == this) instance = null; }
}
}
