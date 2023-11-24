#version 430
out vec4 fragColor;
layout(std430, binding = 2) buffer MyBuffer {
    int dynamicArray[];
};
layout(std430, binding = 4) buffer MyBuffer1 {
    int dynamicArray1[];
};
layout(binding=3) uniform atomic_uint counter;
uniform sampler2D PolyTex;
flat in int col1;
flat in int col2;
void main()
{
    vec4 pix = texelFetch(PolyTex, ivec2(gl_FragCoord.xy), 0);
    int ct = int (ceil(pix.b));

    if(ct !=0) {
        if (dynamicArray1[col2]==0){
            int count = int(atomicCounterIncrement(counter));
            dynamicArray[count] = col1;
            dynamicArray1[col2] = 1;
       }
    }
    fragColor = vec4(0,0,ct,1);
}
