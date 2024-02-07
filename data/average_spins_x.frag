#version 300 es

precision highp float;
precision highp int;
precision highp isampler2D;

uniform isampler2D texture_spin;
uniform vec3 q_colors[10];
uniform int q;
uniform float ds; // size of the spin
uniform float dx; // size of the target cell
uniform float view_min;
uniform float view_max;
uniform float alpha_value;

in vec2 texcoords;
out vec4 color;

void main()
{
    float y = texcoords.y;
    float x = texcoords.x*(view_max-view_min) + view_min;
    float x0 = x - 0.5*dx;
    float x1 = x + 0.5*dx; // upper bound of the cell
    float f0 = mod(x0, ds); // width of ds that is not inside the target cell
    x0 -= f0;

    int spin = texture(texture_spin, vec2(x0 + 0.5*ds, y) ); // get the first spin
    // it is weighted by the fraction of ds which is inside the cell
    vec3 col = q_colors[spin] * ((ds - f0)/ds); //get the color
    float z = x0 + ds; // Increment by one spin width
    while(z < x1)
    {
        spin = texture(texture_spin, vec2(z + 0.5*ds, y) ); // get the spin
        col += q_colors[spin]; //add the color with full weight
        z += ds; // Increment by one spin size
    }
    // Usually the last iteration of above loop adds too much weight for the last spin
    // Here we correct this by subtracting again, this also works if the cell is smaller than one spin
    spin = texture(texture_spin, vec2(z - 0.5*ds, y) ); // get the spin of last iteration
    col -= q_colors[spin] * ((z - x1) / ds); // This should correctly remove weight of the last spin

    col *= (ds/dx); // Correctly normalize the color. This should work out
    // Write result
    float alpha = 1.0;
    if(x0 < 0.0 || x1 > 1.0)
        alpha = alpha_value;
    color = vec4(col,alpha);
}
