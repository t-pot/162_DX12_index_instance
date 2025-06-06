struct Vs2Ps
{
    float4 position : SV_POSITION;
    float4 color : COLOR;// ���C��
};

// �萔�o�b�t�@
cbuffer ObjectConstantBuffer : register(b0)
{
    float4x4 matVP;
};
cbuffer ObjectConstantBuffer : register(b1)
{
    float4x4 matWorld[3*3*3];
};



Vs2Ps VSMain(
    float4 position : POSITION, 
    float4 color    : COLOR, 
    uint instanceID : SV_InstanceID)// ���C��
{
    Vs2Ps output;

    // ���C��
    output.position = mul(mul(position, matWorld[instanceID]), matVP);
    output.color = color;

    return output;
}

float4 PSMain(Vs2Ps input) : SV_TARGET
{
    return input.color; // ���C��
}
