Texture2D gInput : register(t0);
RWTexture2D<float4> gOutput : register(u0);

[numthreads(256, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    // ÏñËØÏà³Ë
    gOutput[DTid.xy] = gOutput[DTid.xy] * gInput[DTid.xy];
}