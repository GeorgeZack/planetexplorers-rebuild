// Pure metadata serialization policy: no Unity dependency or native calls.
using System;
using System.Globalization;
namespace PeReference {
public static class ShieldCaptureNumbers {
    public static string Finite(float value,string context){
        if(Single.IsNaN(value)||Single.IsInfinity(value))
            throw new InvalidOperationException("Nonfinite capture field: "+context+" value="+value);
        return value.ToString("R",CultureInfo.InvariantCulture);
    }
    public static string CurrentClock(float value,int layer,bool transition,float weight,int clipCount,string context,out string reason){
        reason=null;
        // Only informational metadata of a disabled, clipless extra layer may
        // carry Unity's undefined clock sentinel. Base and block stay strict.
        if((Single.IsNaN(value)||Single.IsInfinity(value))&&layer!=0&&layer!=3&&
           !transition&&weight==0&&clipCount==0){reason="inactive-clipless-layer";return "null";}
        return Finite(value,context);
    }
    public static string StateLength(float value,float animatorSpeed,string context,out string reason){
        reason=null;
        Finite(animatorSpeed,context+".animatorSpeed");
        if(Single.IsPositiveInfinity(value)&&animatorSpeed==0){reason="frozen-effective-duration";return "null";}
        return Finite(value,context);
    }
    public static string NextClock(float value,bool transition,string context,out string reason){
        reason=null;
        if(!transition){reason="not-transitioning";return "null";}
        return Finite(value,context);
    }
}
}
