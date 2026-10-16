#version 330 core

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D iChannel0;
uniform vec2 inputSize;
uniform vec2 texScale;

// float hardScan = -8.0;
float hardScan = -4.0;
// float hardPix  = -3.0;
float hardPix  = -2.0;
// vec2  warp     = vec2(1.0/32.0, 1.0/24.0);
// vec2  warp     = vec2(1.0/128.0, 1.0/96.0);
vec2  warp     = vec2(0.0, 0.0); // no curvature
float maskDark = 0.5;
float maskLight = 1.5;
float bloomAmount = 0.15;   // 0 = off
float bloomSpread = 2.0;    // distance between bloom samples, in input pixels
float vignetteStrength = 15.0; // higher = smaller dark corners
float vignettePower = 0.25;    // higher = darker corners

float ToLinear1(float c) { return (c <= 0.04045) ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
vec3  ToLinear(vec3 c) { return vec3(ToLinear1(c.r), ToLinear1(c.g), ToLinear1(c.b)); }
float ToSrgb1(float c) { return (c < 0.0031308 ? c * 12.92 : 1.055 * pow(c, 0.41666) - 0.055); }
vec3  ToSrgb(vec3 c) { return vec3(ToSrgb1(c.r), ToSrgb1(c.g), ToSrgb1(c.b)); }

vec3 Fetch(vec2 pos, vec2 off, vec2 res)
{
    pos = floor(pos * res + off) / res;
    if (max(abs(pos.x - 0.5), abs(pos.y - 0.5)) > 0.5)
        return vec3(0.0);
    return ToLinear(texture(iChannel0, pos * texScale).rgb);
}

vec2 Dist(vec2 pos, vec2 res) { pos = pos * res; return -((pos - floor(pos)) - vec2(0.5)); }
float Gaus(float pos, float scale) { return exp2(scale * pos * pos); }

vec3 Horz3(vec2 pos, float off, vec2 res)
{
    vec3 b = Fetch(pos, vec2(-1.0, off), res);
    vec3 c = Fetch(pos, vec2( 0.0, off), res);
    vec3 d = Fetch(pos, vec2( 1.0, off), res);
    float dst = Dist(pos, res).x;
    float wb = Gaus(dst - 1.0, hardPix);
    float wc = Gaus(dst + 0.0, hardPix);
    float wd = Gaus(dst + 1.0, hardPix);
    return (b * wb + c * wc + d * wd) / (wb + wc + wd);
}

vec3 Horz5(vec2 pos, float off, vec2 res)
{
    vec3 a = Fetch(pos, vec2(-2.0, off), res);
    vec3 b = Fetch(pos, vec2(-1.0, off), res);
    vec3 c = Fetch(pos, vec2( 0.0, off), res);
    vec3 d = Fetch(pos, vec2( 1.0, off), res);
    vec3 e = Fetch(pos, vec2( 2.0, off), res);
    float dst = Dist(pos, res).x;
    float wa = Gaus(dst - 2.0, hardPix);
    float wb = Gaus(dst - 1.0, hardPix);
    float wc = Gaus(dst + 0.0, hardPix);
    float wd = Gaus(dst + 1.0, hardPix);
    float we = Gaus(dst + 2.0, hardPix);
    return (a * wa + b * wb + c * wc + d * wd + e * we) / (wa + wb + wc + wd + we);
}

float Scan(vec2 pos, float off, vec2 res)
{
    float dst = Dist(pos, res).y;
    return Gaus(dst + off, hardScan);
}

vec3 Tri(vec2 pos, vec2 res)
{
    vec3 a = Horz3(pos, -1.0, res);
    vec3 b = Horz5(pos,  0.0, res);
    vec3 c = Horz3(pos,  1.0, res);
    float wa = Scan(pos, -1.0, res);
    float wb = Scan(pos,  0.0, res);
    float wc = Scan(pos,  1.0, res);
    return a * wa + b * wb + c * wc;
}

vec2 Warp(vec2 pos)
{
    pos = pos * 2.0 - 1.0;
    pos *= vec2(1.0 + (pos.y * pos.y) * warp.x, 1.0 + (pos.x * pos.x) * warp.y);
    return pos * 0.5 + 0.5;
}

vec3 Mask(vec2 pos)
{
    pos.x += pos.y * 3.0;
    vec3 mask = vec3(maskDark, maskDark, maskDark);
    pos.x = fract(pos.x / 6.0);
    if (pos.x < 0.333) mask.r = maskLight;
    else if (pos.x < 0.666) mask.g = maskLight;
    else mask.b = maskLight;
    return mask;
}

// Wide 5x5 box blur of the input, added back so bright areas glow
vec3 Bloom(vec2 pos, vec2 res)
{
    vec3 sum = vec3(0.0);
    for (int x = -2; x <= 2; x++)
    for (int y = -2; y <= 2; y++)
    {
        vec2 p = pos + vec2(x, y) * bloomSpread / res;
        sum += ToLinear(texture(iChannel0, clamp(p, 0.0, 1.0) * texScale).rgb);
    }
    return sum / 25.0;
}

// 1 in the middle of the screen, falling off towards the corners
float Vignette(vec2 pos)
{
    pos = pos * (1.0 - pos.yx);
    return pow(clamp(pos.x * pos.y * vignetteStrength, 0.0, 1.0), vignettePower);
}

void main()
{
    vec2 pos = Warp(vUV);
    vec3 col = Tri(pos, inputSize) * Mask(gl_FragCoord.xy);
    col += Bloom(pos, inputSize) * bloomAmount;
    col *= Vignette(pos);
    fragColor = vec4(ToSrgb(col), 1.0);
}