// Offline-compiled groundwork; no Install entry and no automatic launch.
// A future main-thread probe may create these pure clones ONLY from a bundle
// already validated by export_shield_controller.py --check and hash-bound plan.
using System;
using System.IO;
using System.Collections.Generic;
using System.Security.Cryptography;
using UnityEngine;

namespace PeReference {
public sealed class ShieldCompositionClone {
    public struct Parameter {
        public string name;
        public AnimatorControllerParameterType type;
        public float number;
        public bool boolean;
    }
    public struct ClipIdentity {
        public int sourceId;
        public string name;
        public float length, rate;
    }
    const int MaxTransforms=512, MaxClones=16, MaxClips=452;
    const int BaseState=unchecked((int)2858398145u), BlockState=439445811;
    const long MaxBytes=128L*1024*1024;
    static readonly List<ShieldCompositionClone> retained=new List<ShieldCompositionClone>();
    static AssetBundle retainedBundle;
    static RuntimeAnimatorController retainedController;
    static string retainedHash;
    static bool loadAttempted;
    public GameObject root { get; private set; }
    public Animator animator { get; private set; }
    public Transform[] bones { get; private set; }
    public string[] paths { get; private set; }
    bool released;
    static bool Finite(float f){return !Single.IsNaN(f)&&!Single.IsInfinity(f);}
    static string Hash(byte[] bytes){using(SHA256 sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-","").ToLowerInvariant();}
    static RuntimeAnimatorController Controller(string path,string expectedHash,ClipIdentity[] expected,Action<string> diagnostic){
        if(String.IsNullOrEmpty(path)||!Path.IsPathRooted(path)||expectedHash==null||expectedHash.Length!=64)
            throw new ArgumentException("Absolute validated bundle path and SHA256 required");
        if(retainedController!=null){if(retainedHash!=expectedHash)throw new InvalidOperationException("One immutable bundle per process");return retainedController;}
        if(loadAttempted)throw new InvalidOperationException("A failed native load cannot be retried in this process");
        FileInfo info=new FileInfo(path);if(!info.Exists||info.Length<=0||info.Length>MaxBytes)throw new InvalidOperationException("Bundle size bound");
        byte[] bytes=File.ReadAllBytes(path);if(Hash(bytes)!=expectedHash)throw new InvalidOperationException("Bundle hash differs from validated plan");
        // Unity5.2's CreateFromFile only accepts uncompressed bundles. The
        // retained UnityWeb source container uses CreateFromMemoryImmediate.
        loadAttempted=true;
        retainedBundle=AssetBundle.CreateFromMemoryImmediate(bytes);
        if(retainedBundle==null)throw new InvalidOperationException("Unity rejected prepared bundle");
        retainedHash=expectedHash;
        UnityEngine.Object[] assets=retainedBundle.LoadAllAssets();
        diagnostic("LoadAllAssets count="+assets.Length);
        for(int i=0;i<Math.Min(assets.Length,16);i++)diagnostic("LoadAllAssets["+i+"] type="+(assets[i]==null?"null":assets[i].GetType().FullName)+" name="+(assets[i]==null?"null":assets[i].name));
        // This bundle exports exactly one m_Container entry. Dependencies are
        // validated through the named controller, not LoadAllAssets enumeration.
        RuntimeAnimatorController candidate=retainedBundle.LoadAsset("reference_shield_controller",typeof(RuntimeAnimatorController)) as RuntimeAnimatorController;
        if(candidate==null||candidate.name!="ReferenceShieldCompositionNoCallbacks")throw new InvalidOperationException("Named controller container missing or wrong");
        if(assets.Length!=1||assets[0]!=candidate)throw new InvalidOperationException("Expected exactly the one named controller container");
        if(expected==null||expected.Length!=MaxClips)throw new InvalidOperationException("Compiled source clip allowlist differs");
        AnimationClip[] references=candidate.animationClips;
        diagnostic("Controller animationClips references="+references.Length+" expectedDistinct="+MaxClips);
        if(references.Length<MaxClips||references.Length>608)throw new InvalidOperationException("Controller clip reference bound differs");
        bool[] matched=new bool[MaxClips];HashSet<int> identities=new HashSet<int>();
        foreach(AnimationClip clip in references){
            if(clip==null||clip.events.Length!=0||clip.legacy||!Finite(clip.length)||!Finite(clip.frameRate))
                throw new InvalidOperationException("Unsafe controller clip event/type/timing");
            if(!identities.Add(clip.GetInstanceID()))continue;
            int found=-1;
            for(int i=0;i<expected.Length;i++)if(!matched[i]&&clip.name==expected[i].name&&
                    Math.Abs(clip.length-expected[i].length)<=0.00001f&&clip.frameRate==expected[i].rate){found=i;break;}
            if(found<0)throw new InvalidOperationException("Clip outside compiled source allowlist: "+clip.name+" length="+clip.length+" rate="+clip.frameRate);
            matched[found]=true;
        }
        if(identities.Count!=MaxClips)throw new InvalidOperationException("Controller distinct clip references differ: "+identities.Count);
        for(int i=0;i<matched.Length;i++)if(!matched[i])throw new InvalidOperationException("Missing source clip "+expected[i].sourceId+" "+expected[i].name);
        diagnostic("Validated named controller and452 distinct event-free clips against compiled source names/lengths/rates");
        retainedController=candidate;
        return retainedController;
    }
    public static ShieldCompositionClone Create(Animator original,string bundlePath,string expectedHash,ClipIdentity[] expected,Action<string> diagnostic){
        if(diagnostic==null)throw new ArgumentNullException("diagnostic");
        if(original==null||!original.enabled||!original.gameObject.activeInHierarchy||original.avatar==null||
           !original.avatar.isValid||!original.isHuman||original.avatar.name!="FemaleAvatar")
            throw new ArgumentException("Active source female Avatar required");
        if(retained.Count>=MaxClones)throw new InvalidOperationException("Retained clone bound");
        RuntimeAnimatorController controller=Controller(bundlePath,expectedHash,expected,diagnostic);
        Transform[] source=original.transform.GetComponentsInChildren<Transform>(true);
        if(source.Length<2||source.Length>MaxTransforms)throw new InvalidOperationException("Source skeleton bound");
        ShieldCompositionClone clone=new ShieldCompositionClone();retained.Add(clone);
        try {
            clone.root=new GameObject("ReferenceShieldComposition_"+retained.Count);clone.root.hideFlags=HideFlags.HideAndDontSave;
            clone.root.SetActive(false);UnityEngine.Object.DontDestroyOnLoad(clone.root);
            clone.bones=new Transform[source.Length];clone.paths=new string[source.Length];
            Dictionary<Transform,int> map=new Dictionary<Transform,int>();HashSet<string> paths=new HashSet<string>();
            for(int i=0;i<source.Length;i++){
                int parent=-1;Transform s=source[i],t;
                if(i==0){if(s!=original.transform)throw new InvalidOperationException("Source root order");t=clone.root.transform;clone.paths[i]="";}
                else {
                    if(!map.TryGetValue(s.parent,out parent))throw new InvalidOperationException("Source parent order");
                    GameObject child=new GameObject(s.name);child.hideFlags=HideFlags.HideAndDontSave;t=child.transform;t.parent=clone.bones[parent];
                    clone.paths[i]=clone.paths[parent].Length==0?s.name:clone.paths[parent]+"/"+s.name;
                }
                if(!paths.Add(clone.paths[i]))throw new InvalidOperationException("Duplicate source path");
                clone.bones[i]=t;map.Add(s,i);
                t.localPosition=i==0?Vector3.zero:s.localPosition;
                t.localRotation=i==0?Quaternion.identity:s.localRotation;
                t.localScale=i==0?Vector3.one:s.localScale;
            }
            clone.animator=clone.root.AddComponent<Animator>();
            clone.animator.avatar=original.avatar;
            clone.animator.fireEvents=false;clone.animator.applyRootMotion=false;
            clone.animator.cullingMode=AnimatorCullingMode.AlwaysAnimate;clone.animator.speed=0;
            // Never assign original.runtimeAnimatorController. Offline validation
            // removed every behaviour before this distinct controller can load.
            clone.animator.runtimeAnimatorController=controller;
            clone.AssertPure();clone.root.SetActive(true);clone.animator.Rebind();clone.AssertPure();
            if(clone.animator.GetLayerName(0)!="Base Layer"||clone.animator.GetLayerName(3)!="LArmOnly")
                throw new InvalidOperationException("Source layer identity differs");
            return clone;
        } catch {clone.Release();throw;}
    }
    public void Seek(Parameter[] parameters,float baseNormalizedTime,float blockNormalizedTime){
        if(released||parameters==null||parameters.Length>512||!Finite(baseNormalizedTime)||!Finite(blockNormalizedTime)||
           baseNormalizedTime<0||blockNormalizedTime<0||baseNormalizedTime>1000000||blockNormalizedTime>1000000)
            throw new ArgumentException("Invalid bounded phase/parameters");
        AssertPure();Dictionary<string,AnimatorControllerParameterType> allowed=new Dictionary<string,AnimatorControllerParameterType>();
        foreach(AnimatorControllerParameter p in animator.parameters)allowed.Add(p.name,p.type);
        HashSet<string> names=new HashSet<string>();
        foreach(Parameter p in parameters){
            AnimatorControllerParameterType type;
            if(String.IsNullOrEmpty(p.name)||!names.Add(p.name)||!allowed.TryGetValue(p.name,out type)||type!=p.type||
               type==AnimatorControllerParameterType.Trigger||!Finite(p.number))throw new ArgumentException("Invalid replay parameter");
            if(type==AnimatorControllerParameterType.Int&&(p.number<Int32.MinValue||p.number>=2147483648f||p.number!=(float)(int)p.number))
                throw new ArgumentException("Invalid integer replay parameter");
        }
        foreach(Parameter p in parameters){
            if(p.type==AnimatorControllerParameterType.Float)animator.SetFloat(p.name,p.number);
            else if(p.type==AnimatorControllerParameterType.Int)animator.SetInteger(p.name,(int)p.number);
            else if(p.type==AnimatorControllerParameterType.Bool)animator.SetBool(p.name,p.boolean);
        }
        if(!animator.GetBool("SheildBlock")||animator.GetFloat("FirstPerson")!=0)
            throw new InvalidOperationException("Replay requires actual third-person block evidence");
        for(int i=0;i<animator.layerCount;i++)animator.SetLayerWeight(i,i==3?1:0);
        animator.Play(BaseState,0,baseNormalizedTime);animator.Play(BlockState,3,blockNormalizedTime);
        animator.Update(0f);AssertPure();
        // Caller MUST wait a full player-loop/EOF before reading matrices, then
        // record actual layer clocks/weights and compare to the source frame.
    }
    public void AssertPure(){
        if(root==null||animator==null||animator.runtimeAnimatorController!=retainedController||
           animator.fireEvents||animator.applyRootMotion||animator.speed!=0||
           animator.GetBehaviours<StateMachineBehaviour>().Length!=0)throw new InvalidOperationException("Clone isolation changed");
        foreach(Transform t in bones)foreach(Component c in t.GetComponents<Component>())
            if(!(c is Transform)&&c!=animator)throw new InvalidOperationException("Nonpure clone component");
    }
    public void Release(){
        if(released)return;released=true;
        if(animator!=null)animator.enabled=false;if(root!=null)root.SetActive(false);
        // Keep inactive clones and the bundle strongly referenced until this
        // disposable process exits; no graph disposal, Destroy or bundle Unload.
    }
}
}
