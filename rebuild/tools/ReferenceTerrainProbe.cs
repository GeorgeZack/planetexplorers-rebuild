// Read-only loaded-terrain snapshot; uses the established pose shim entry point.
using System;
using System.IO;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
namespace PeReference {
public sealed class PoseProbe : MonoBehaviour {
    static PoseProbe instance;
    string output, arm, stop;
    float installed, started = -1;
    byte[] previous;
    bool finished;
    const int X = -627, Y = 16, Z = 323, SX = 64, SY = 32, SZ = 128;
    public static void Install() {
        if (instance != null) return;
        GameObject host = new GameObject("ReferenceTerrainProbe (read only)");
        host.hideFlags = HideFlags.HideAndDontSave;
        DontDestroyOnLoad(host);
        instance = host.AddComponent<PoseProbe>();
        instance.output = Environment.GetEnvironmentVariable("PE_POSE_OUTPUT");
        instance.arm = Environment.GetEnvironmentVariable("PE_POSE_ARM");
        instance.stop = Environment.GetEnvironmentVariable("PE_POSE_STOP");
        if (String.IsNullOrEmpty(instance.output) || !Path.IsPathRooted(instance.output)) {
            Destroy(host); throw new ArgumentException("Absolute output required");
        }
        instance.installed = Time.realtimeSinceStartup;
        Console.Error.WriteLine("[terrain] Waiting for arm file: " + instance.arm);
        instance.StartCoroutine(instance.Capture());
    }
    IEnumerator Capture() {
        while (!finished) {
            yield return new WaitForEndOfFrame();
            try { Tick(); }
            catch (Exception e) { Console.Error.WriteLine("[terrain] Failed: " + e); finished = true; }
        }
        Destroy(gameObject);
    }
    void Tick() {
        float now = Time.realtimeSinceStartup;
        if (File.Exists(stop) || now-installed > 1800 || (started >= 0 && now-started > 120)) {
            Console.Error.WriteLine("[terrain] Stopped without complete stable crop"); finished = true; return;
        }
        if (!File.Exists(arm)) return;
        if (started < 0) started = now;
        VFVoxelTerrain terrain = VFVoxelTerrain.self;
        if (terrain == null || terrain.Voxels == null) return;
        VFLODDataSource source = terrain.Voxels as VFLODDataSource;
        if (source == null) throw new InvalidOperationException("Expected VFLODDataSource");
        const int shift = VoxelTerrainConstants._shift;
        Dictionary<string, byte[]> chunks = new Dictionary<string, byte[]>();
        for (int cz = Z >> shift; cz <= (Z+SZ-1) >> shift; cz++)
        for (int cy = Y >> shift; cy <= (Y+SY-1) >> shift; cy++)
        for (int cx = X >> shift; cx <= (X+SX-1) >> shift; cx++) {
            VFVoxelChunkData chunk = source.readChunk(cx, cy, cz, 0);
            if (chunk == null || chunk.DataVT == null || chunk.DataVT.Length == 0) { previous = null; return; }
            byte[] data = (byte[])chunk.DataVT.Clone();
            if (data.Length != 2 && data.Length != VoxelTerrainConstants.VOXEL_ARRAY_LENGTH_VT)
                throw new InvalidOperationException("Unexpected terrain chunk length: " + data.Length);
            chunks.Add(cx+","+cy+","+cz, data);
        }
        byte[] voxels = new byte[SX*SY*SZ*2];
        int index = 0;
        for (int z=Z; z<Z+SZ; z++) for (int y=Y; y<Y+SY; y++) for (int x=X; x<X+SX; x++) {
            byte[] data = chunks[(x>>shift)+","+(y>>shift)+","+(z>>shift)];
            int offset = data.Length == 2 ? 0 : VFVoxelChunkData.OneIndex(x & VoxelTerrainConstants._mask, y & VoxelTerrainConstants._mask, z & VoxelTerrainConstants._mask)*2;
            voxels[index++] = data[offset]; voxels[index++] = data[offset+1];
        }
        bool equal = previous != null;
        if (equal) for (int i=0; i<voxels.Length; i++) if (voxels[i] != previous[i]) { equal=false; break; }
        previous = voxels;
        if (!equal) return;
        using (BinaryWriter writer = new BinaryWriter(new FileStream(output, FileMode.CreateNew, FileAccess.Write))) {
            writer.Write(new byte[]{80,69,86,88}); writer.Write(1);
            writer.Write(X); writer.Write(Y); writer.Write(Z); writer.Write(SX); writer.Write(SY); writer.Write(SZ);
            writer.Write(voxels);
        }
        string metadata = "{\"schema\":1,\"kind\":\"observed-original-loaded-terrain\",\"origin\":[-627,16,323],\"size\":[64,32,128],\"unity\":\""+Application.unityVersion+"\",\"frame\":"+Time.frameCount+",\"stableConsecutiveFrames\":2,\"loadedChunks\":"+chunks.Count+",\"layout\":\"x-fastest,y,z;volume,type\"}";
        using (StreamWriter writer = new StreamWriter(new FileStream(output+".json", FileMode.CreateNew, FileAccess.Write))) writer.WriteLine(metadata);
        Console.Error.WriteLine("[terrain] Captured complete stable terrain: " + output);
        finished = true;
    }
}
}
