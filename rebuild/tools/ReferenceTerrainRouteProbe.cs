// Read-only route capture. Only the desktop operator drives the original.
using System;
using System.IO;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using System.Globalization;
using UnityEngine;
namespace PeReference {
public sealed class PoseProbe : MonoBehaviour {
    static PoseProbe instance;
    string output, arm, stop, directory;
    float installed, started = -1, limit, reported = -1;
    int[] origin, size;
    bool finished;
    readonly Dictionary<string, byte[]> previous = new Dictionary<string, byte[]>();
    readonly Dictionary<string, byte[]> captured = new Dictionary<string, byte[]>();
    readonly Dictionary<string, int> frames = new Dictionary<string, int>();
    static int[] Triple(string name) {
        string[] parts = Environment.GetEnvironmentVariable(name).Split(',');
        if (parts.Length != 3) throw new ArgumentException(name);
        return new int[]{Int32.Parse(parts[0]),Int32.Parse(parts[1]),Int32.Parse(parts[2])};
    }
    public static void Install() {
        if (instance != null) return;
        GameObject host = new GameObject("ReferenceTerrainRouteProbe (read only)");
        host.hideFlags = HideFlags.HideAndDontSave; DontDestroyOnLoad(host);
        instance = host.AddComponent<PoseProbe>();
        instance.output = Environment.GetEnvironmentVariable("PE_POSE_OUTPUT");
        instance.arm = Environment.GetEnvironmentVariable("PE_POSE_ARM");
        instance.stop = Environment.GetEnvironmentVariable("PE_POSE_STOP");
        instance.origin = Triple("PE_TERRAIN_ORIGIN"); instance.size = Triple("PE_TERRAIN_SIZE");
        instance.limit = Int32.Parse(Environment.GetEnvironmentVariable("PE_TERRAIN_SECONDS"));
        if (String.IsNullOrEmpty(instance.output) || !Path.IsPathRooted(instance.output)) {
            Destroy(host); throw new ArgumentException("Absolute output required");
        }
        instance.directory = instance.output + ".chunks";
        if (Directory.Exists(instance.directory) || File.Exists(instance.output)) {
            Destroy(host); throw new IOException("Capture destination already exists");
        }
        instance.installed = Time.realtimeSinceStartup;
        Console.Error.WriteLine("[terrain-route] Waiting for arm file: " + instance.arm);
        instance.StartCoroutine(instance.Capture());
    }
    IEnumerator Capture() {
        while (!finished) {
            yield return new WaitForEndOfFrame();
            try { Tick(); }
            catch (Exception e) { Console.Error.WriteLine("[terrain-route] Failed: " + e); finished = true; }
        }
        Destroy(gameObject);
    }
    static string Key(int x, int y, int z) { return x+","+y+","+z; }
    static string Quote(string value) { return "\""+(value??"").Replace("\\","\\\\").Replace("\"","\\\"").Replace("\n","\\n").Replace("\r","\\r").Replace("\t","\\t")+"\""; }
    static bool Equal(byte[] a, byte[] b) {
        if (a.Length != b.Length) return false;
        for(int i=0;i<a.Length;i++) if(a[i]!=b[i]) return false;
        return true;
    }
    static void Header(BinaryWriter writer, int[] o, int[] s) {
        writer.Write(new byte[]{80,69,86,88}); writer.Write(1);
        foreach(int n in o) writer.Write(n); foreach(int n in s) writer.Write(n);
    }
    static byte[] Samples(byte[] data) {
        int side = 1 << VoxelTerrainConstants._shift;
        byte[] samples = new byte[side*side*side*2]; int index=0;
        for(int z=0;z<side;z++) for(int y=0;y<side;y++) for(int x=0;x<side;x++) {
            int at = data.Length==2?0:VFVoxelChunkData.OneIndex(x,y,z)*2;
            samples[index++]=data[at]; samples[index++]=data[at+1];
        }
        return samples;
    }
    void Manifest(int total, bool complete) {
        string metadata="{\"schema\":1,\"kind\":\"observed-original-loaded-terrain-route\",\"origin\":["+String.Join(",",Array.ConvertAll(origin, x=>x.ToString()))+"],\"size\":["+String.Join(",",Array.ConvertAll(size,x=>x.ToString()))+"],\"unity\":\""+Application.unityVersion+"\",\"stableConsecutiveFramesPerChunk\":2,\"simultaneousSnapshot\":false,\"complete\":"+(complete?"true":"false")+",\"requiredChunks\":"+total+",\"capturedChunks\":"+captured.Count+",\"layout\":\"x-fastest,y,z;volume,type\",\"frames\":{";
        bool first=true; foreach(KeyValuePair<string,int> pair in frames) {
            if(!first) metadata+=","; first=false; metadata+="\""+pair.Key+"\":"+pair.Value;
        }
        metadata+="}";
        foreach(Assembly assembly in AppDomain.CurrentDomain.GetAssemblies()) {
            Type type=assembly.GetType("Pathea.MainPlayerCmpt"); if(type==null)continue;
            FieldInfo field=type.GetField("gMainPlayer",BindingFlags.Public|BindingFlags.Static);
            Component player=field==null?null:field.GetValue(null) as Component;
            if(player!=null) {
                Component trans=player.GetComponent("PeTrans");
                if(trans!=null) {
                    PropertyInfo position=trans.GetType().GetProperty("position"), rotation=trans.GetType().GetProperty("rotation");
                    if(position!=null&&rotation!=null) {
                        Vector3 p=(Vector3)position.GetValue(trans,null);
                        Quaternion q=(Quaternion)rotation.GetValue(trans,null);
                        metadata+=",\"player\":["+p.x.ToString("R",CultureInfo.InvariantCulture)+","+p.y.ToString("R",CultureInfo.InvariantCulture)+","+p.z.ToString("R",CultureInfo.InvariantCulture)+"],\"playerYaw\":"+q.eulerAngles.y.ToString("R",CultureInfo.InvariantCulture);
                    }
                }
            }
            break;
        }
        Camera camera=Camera.main;
        if(camera!=null)metadata+=",\"cameraYaw\":"+camera.transform.eulerAngles.y.ToString("R",CultureInfo.InvariantCulture);
        VArtifactTownManager towns=VArtifactTownManager.Instance;
        if(towns!=null) {
            metadata+=",\"towns\":["; bool firstTown=true;
            foreach(int id in new int[]{0,4}) {
                VArtifactTown town=towns.GetTownByID(id);if(town==null)continue;
                if(!firstTown)metadata+=",";firstTown=false;
                metadata+="{\"id\":"+id+",\"nameId\":"+town.townNameId+",\"name\":"+Quote(PELocalization.GetString(town.townNameId))+",\"centerXZ\":["+town.PosCenter.x+","+town.PosCenter.y+"],\"startXZ\":["+town.PosStart.x+","+town.PosStart.y+"],\"endXZ\":["+town.PosEnd.x+","+town.PosEnd.y+"],\"height\":"+town.height+"}";
            }
            metadata+="]";
        }
        metadata+="}";
        string path=directory+"/manifest.json", temporary=path+".pending";
        File.WriteAllText(temporary,metadata+"\n");
        if(File.Exists(path)) File.Delete(path); File.Move(temporary,path);
    }
    void Tick() {
        float now=Time.realtimeSinceStartup;
        if(File.Exists(stop)||now-installed>3600||(started>=0&&now-started>limit)) {
            Console.Error.WriteLine("[terrain-route] Stopped; preserved "+captured.Count+" stable chunks in "+directory); finished=true; return;
        }
        if(!File.Exists(arm)) return;
        if(started<0) { started=now; Directory.CreateDirectory(directory); }
        VFVoxelTerrain terrain=VFVoxelTerrain.self;
        if(terrain==null||terrain.Voxels==null) return;
        VFLODDataSource source=terrain.Voxels as VFLODDataSource;
        if(source==null) throw new InvalidOperationException("Expected VFLODDataSource");
        const int shift=VoxelTerrainConstants._shift;
        int total=0, added=0;
        for(int cz=origin[2]>>shift;cz<=(origin[2]+size[2]-1)>>shift;cz++)
        for(int cy=origin[1]>>shift;cy<=(origin[1]+size[1]-1)>>shift;cy++)
        for(int cx=origin[0]>>shift;cx<=(origin[0]+size[0]-1)>>shift;cx++) {
            total++; string key=Key(cx,cy,cz); if(captured.ContainsKey(key)) continue;
            VFVoxelChunkData chunk=source.readChunk(cx,cy,cz,0);
            if(chunk==null||chunk.DataVT==null||chunk.DataVT.Length==0) { previous.Remove(key); continue; }
            byte[] data=(byte[])chunk.DataVT.Clone();
            if(data.Length!=2&&data.Length!=VoxelTerrainConstants.VOXEL_ARRAY_LENGTH_VT) throw new InvalidOperationException("Unexpected chunk length");
            byte[] prior; bool stable=previous.TryGetValue(key,out prior)&&Equal(prior,data);
            if(!stable) { previous[key]=data; continue; }
            byte[] samples=Samples(data); captured.Add(key,samples); frames.Add(key,Time.frameCount); previous.Remove(key); added++;
            using(BinaryWriter writer=new BinaryWriter(new FileStream(directory+"/"+key+".pevx",FileMode.CreateNew,FileAccess.Write))) {
                int side=1<<shift; Header(writer,new int[]{cx<<shift,cy<<shift,cz<<shift},new int[]{side,side,side}); writer.Write(samples);
            }
        }
        if(added>0||now-reported>=1) { Manifest(total,captured.Count==total); reported=now; }
        if(added>0) Console.Error.WriteLine("[terrain-route] Stable chunks "+captured.Count+"/"+total);
        if(captured.Count!=total) return;
        using(BinaryWriter writer=new BinaryWriter(new FileStream(output,FileMode.CreateNew,FileAccess.Write))) {
            Header(writer,origin,size); int side=1<<shift, mask=side-1;
            for(int z=origin[2];z<origin[2]+size[2];z++) for(int y=origin[1];y<origin[1]+size[1];y++) for(int x=origin[0];x<origin[0]+size[0];x++) {
                byte[] data=captured[Key(x>>shift,y>>shift,z>>shift)]; int at=(((z&mask)*side+(y&mask))*side+(x&mask))*2;
                writer.Write(data[at]); writer.Write(data[at+1]);
            }
        }
        File.Copy(directory+"/manifest.json",output+".json",false);
        Console.Error.WriteLine("[terrain-route] Complete captured rectangle: "+output); finished=true;
    }
}
}
