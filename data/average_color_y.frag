#version 300 es

precision highp float;
precision highp int;
precision highp sampler2D;

uniform sampler2D texture_x_averaged;
uniform float dy; // size of the target pixel in units of ds
uniform float ds; // size of the spin. We do not need it / should set it to 1
uniform float view_min;
uniform float view_max;
uniform float alpha_value;

in vec2 texcoords;
out vec3 color;

void main()
{
    vec4 col;
    float x = texcoords.x;
    float y = texcoords.y * (view_max - view_min) + view_min;
    float y0 = y - 0.5*dy;
    float y1 = y + 0.5*dy; // upper bound of the cell
    float f0 = mod(y0, ds); // width of ds that is not inside the target pixel
    y0 -= f0;

    float weight = ((ds - f0)/ds);
    col = texture(texture_x_averaged, vec2(x, y0 + 0.5*ds) ) * weight; // get the first cell
    // it is weighted by the fraction of ds which is inside the pixel

    float z = y0 + ds; // Increment by one spin width
    while(z < y1)
    {
        col += texture(texture_x_averaged, vec2(x, z + 0.5*ds) ); //add the color with full weight
        z += ds; // Increment by one spin size
    }
    // Usually the last iteration of above loop adds too much weight for the last cell
    // Here we correct this by subtracting again, this also works if the cell is smaller than one spin
    col -= texture(texture_x_averaged, vec2(x, z - 0.5*ds) ) * ((z - y1) / ds); // This should correctly remove weight of the last cell
    col *= (ds/dy); // Correctly normalize the color. This should work out
    float alpha = col.w;
    if(y0 < 0.0 || y1 > 1.0)
        alpha = alpha_value;
    color = alpha * col.xyz;
}
