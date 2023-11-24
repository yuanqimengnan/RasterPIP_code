#version 460
out vec4 fragColor;
layout(std430, binding = 2) buffer MyBuffer {
    int dynamicArray[];
};
layout(std430, binding = 4) buffer MyBuffer1 {
    int dynamicArray1[];
};
//layout(std430, binding = 5) buffer MyBuffer2 {
//    int dynamicArray2[];
//};
//layout(std430, binding = 6) buffer MyBuffer3 {
//    int dynamicArray3[];
//};
//layout(std430, binding = 7) buffer MyBuffer4 {
//    int dynamicArray4[];
//};
//layout(std430, binding = 8) buffer MyBuffer5 {
//    int dynamicArray5[];
//};
layout(binding=3) uniform atomic_uint counter;
uniform sampler2D PolyTex;
flat in int col1;
flat in int col2;
void main()
{
    vec4 pix = texelFetch(PolyTex, ivec2(gl_FragCoord.xy), 0);
    int r = int (ceil(pix.r));
    int g = int (ceil(pix.g));
    int b = int (ceil(pix.b));
    int a = int (ceil(pix.a));
    if((r|g|b|a)!=0) {
        if (dynamicArray1[col2] ==0) {
            int count = int( atomicCounterAdd(counter, 5));
            dynamicArray[count] = col1;
            dynamicArray[count+1] = r;
            dynamicArray[count+2] = g;
            dynamicArray[count+3] = b;
            dynamicArray[count+4] = a;
            dynamicArray1[col2] = 1;
       }
    }
    fragColor = vec4(0,0,1,1);
}
