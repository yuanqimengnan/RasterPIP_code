#version 430
uniform mat4 mvpMatrix;
flat out int col1;
flat out int col2;
layout(location=0) in  vec2 vertex;
layout(location=1) in  int id;
layout(std430, binding = 4) buffer MyBuffer1 {
    int dynamicArray1[];
};
uniform bool clear_flag;

void main()
{
    gl_Position = mvpMatrix * vec4(vertex,0,1);
    col1  = id;
    col2 = int(gl_VertexID);
    if (clear_flag) {
        dynamicArray1[col2] = 0;
    }
}

