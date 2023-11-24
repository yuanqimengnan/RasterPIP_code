#version 430

out vec4 fragColor;
in float col;

void main()
{
    int divisor = 31;
    int Quotient = int(col) / divisor;
    int remander = int(col) % divisor;
    float id = pow(2.0, remander);
    switch (Quotient) {
    case 0:
        fragColor = vec4(id,0,0,0);
        break;
    case 1:
        fragColor = vec4(0,id,0,0);
        break;
    case 2:
        fragColor = vec4(0,0,id,0);
        break;
    case 3:
        fragColor = vec4(0,0,0,id);
        break;
    default:
        fragColor = vec4(0,0,0,0);
        break;
    }
}
