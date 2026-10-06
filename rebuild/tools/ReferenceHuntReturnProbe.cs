// Observation only: existing return mission, inventory and talk fields. No input/gameplay/save calls.
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
    FieldInfo packageField, talkIndexField, slotsField, itemsField;
    const long MaxBytes = 16L * 1024 * 1024;
    static readonly CultureInfo C = CultureInfo.InvariantCulture;
    public static void Install() {
        if (instance != null) return;
        GameObject host = new GameObject("ReferenceHuntReturnProbe (observation only)");
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
        maxFrames = Setting("PE_POSE_FRAMES", 600, 1, 3600);
        seconds = Setting("PE_POSE_SECONDS", 20, 1, 120);
        waitSeconds = Setting("PE_POSE_WAIT", 900, 1, 1800);
        packageField = Field(typeof(PlayerPackageCmpt), "mPackage");
        talkIndexField = Field(typeof(UINPCTalk), "m_CurTalkIdx");
        slotsField = Field(typeof(ItemAsset.ItemPackage), "mSlotListArray");
        itemsField = Field(typeof(ItemAsset.SlotList), "mSlots");
        output = new StreamWriter(new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read), new UTF8Encoding(false));
        installed = Time.realtimeSinceStartup;
        Write("{\"type\":\"header\",\"schema\":1,\"kind\":\"observed-original-hunt-return\",\"mission\":9127,\"target\":4525,\"phase\":\"WaitForEndOfFrame\",\"unity\":" + Q(Application.unityVersion) +
              ",\"probeSourceSha256\":" + Q(Environment.GetEnvironmentVariable("PE_HUNT_RETURN_PROBE_SHA256")) + ",\"maxFrames\":" + maxFrames + ",\"maxBytes\":" + MaxBytes + "}");
        output.Flush();
        Console.Error.WriteLine("[hunt] Waiting for arm file: " + arm);
        StartCoroutine(Capture());
    }
    IEnumerator Capture() {
        while (!finished) {
            yield return new WaitForEndOfFrame();
            try { Tick(); }
            catch (Exception error) { Console.Error.WriteLine("[hunt] Failed: " + error); Finish("error"); }
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
        // Never call PlayerPackageCmpt.package: it creates a missing package.
        MainPlayerCmpt main = MainPlayerCmpt.gMainPlayer;
        PeEntity player = main == null ? null : main.GetComponent<PeEntity>();
        PlayerPackageCmpt component = player == null ? null : player.GetComponent<PlayerPackageCmpt>();
        PlayerPackage package = component == null ? null : packageField.GetValue(component) as PlayerPackage;
        MissionManager manager = MissionManager.Instance;
        PlayerMission missions = manager == null ? null : manager.m_PlayerMission;
        GameUI ui = GameUI.Instance;
        UINPCTalk talk = ui == null ? null : ui.mNPCTalk;
        StringBuilder b = new StringBuilder(2048);
        b.Append("{\"type\":\"frame\",\"sample\":").Append(frames).Append(",\"frame\":").Append(Time.frameCount)
         .Append(",\"time\":").Append(F(Time.time)).Append(",\"realtime\":").Append(F(now))
         .Append(",\"player\":").Append(player == null ? "null" : "{\"id\":" + player.Id + ",\"position\":" + V(player.position) + "}")
         .Append(",\"missions\":").Append(missions == null ? "null" : "{\"9127\":" + Mission(missions, 9127) + ",\"9077\":" + Mission(missions, 9077) + "}")
         .Append(",\"inventory\":").Append(Inventory(package == null ? null : package._playerPak))
         .Append(",\"missionInventory\":").Append(Inventory(package == null ? null : PlayerPackage._missionPak))
         .Append(",\"talk\":").Append(Talk(talk)).Append('}');
        Write(b.ToString()); output.Flush(); frames++;
    }
    static FieldInfo Field(Type type, string name) {
        FieldInfo field = type.GetField(name, BindingFlags.NonPublic | BindingFlags.Instance);
        if (field == null) throw new InvalidOperationException("Missing existing field " + type.Name + "." + name);
        return field;
    }
    static string Mission(PlayerMission p, int id) {
        int completed = 0;
        if (p.m_MissionState == null) throw new InvalidOperationException("Missing mission state");
        p.m_MissionState.TryGetValue(id, out completed);
        return "{\"active\":" + (p.HasMission(id) ? "true" : "false") + ",\"completed\":" + completed + "}";
    }
    string Inventory(ItemAsset.ItemPackage package) {
        if (package == null) return "null";
        ItemAsset.SlotList[] lists = slotsField.GetValue(package) as ItemAsset.SlotList[];
        if (lists == null || lists.Length > 16) throw new InvalidOperationException("Invalid existing slot groups");
        int[] counts = new int[4]; int slots = 0;
        foreach (ItemAsset.SlotList list in lists) {
            if (list == null) continue;
            ItemAsset.ItemObject[] items = itemsField.GetValue(list) as ItemAsset.ItemObject[];
            if (items == null) return "null"; // A partially deserialized package is not zero inventory.
            slots += items.Length;
            if (slots > 8192) throw new InvalidOperationException("Inventory observation bound");
            foreach (ItemAsset.ItemObject item in items) {
                if (item == null || item.protoId < 592 || item.protoId > 595) continue;
                if (item.stackCount < 0) throw new InvalidOperationException("Negative observed stack");
                checked { counts[item.protoId - 592] += item.stackCount; }
            }
        }
        return "{\"592\":" + counts[0] + ",\"593\":" + counts[1] + ",\"594\":" + counts[2] + ",\"595\":" + counts[3] + "}";
    }
    string Talk(UINPCTalk talk) {
        if (talk == null) return "null";
        int index = (int)talkIndexField.GetValue(talk);
        if (talk.m_NpcTalkList == null || talk.m_NpcTalkList.Count > 256) throw new InvalidOperationException("Invalid talk list");
        StringBuilder b = new StringBuilder();
        b.Append("{\"active\":").Append(talk.gameObject.activeInHierarchy ? "true" : "false")
         .Append(",\"playing\":").Append(talk.isPlayingTalk ? "true" : "false")
         .Append(",\"canClose\":").Append(talk.canClose ? "true" : "false")
         .Append(",\"mode\":").Append((int)talk.type).Append(",\"index\":").Append(index)
         .Append(",\"selectionMission\":").Append(talk.m_selectMissionSource)
         .Append(",\"npcEntity\":").Append(talk.m_CurTalkNpc == null ? "null" : talk.m_CurTalkNpc.Id.ToString(C))
         .Append(",\"currentId\":").Append(index >= 0 && index < talk.m_NpcTalkList.Count ? talk.m_NpcTalkList[index].talkid.ToString(C) : "null")
         .Append(",\"pages\":[");
        for (int i = 0; i < talk.m_NpcTalkList.Count; i++) {
            if (i != 0) b.Append(',');
            UINPCTalk.NpcTalkInfo page = talk.m_NpcTalkList[i];
            b.Append("{\"id\":").Append(page.talkid).Append(",\"npc\":").Append(page.npcid)
             .Append(",\"missionTrigger\":").Append(page.missionTrigger).Append(",\"takeType\":").Append((int)page.type).Append('}');
        }
        return b.Append("]}").ToString();
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
        Console.Error.WriteLine("[hunt] Finished: " + reason);
    }
    void OnDestroy() { Finish("destroy"); }
}
}
