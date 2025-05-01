#include "framework.h" // Windowsの標準的なinclude
#include "Application.h"
#include <random>

namespace tpot
{
	Application::Application(const ApplicationArgs& 引数) : 
		幅_(引数.幅), 高さ_(引数.高さ), Warpデバイス使用_(引数.useWarpDevice),
		アスペクト比_(static_cast<float>(引数.幅) / static_cast<float>(引数.高さ)),
        ビューポート_(0.0f, 0.0f, static_cast<float>(引数.幅), static_cast<float>(引数.高さ)),
        はさみ矩形_(0, 0, static_cast<LONG>(引数.幅), static_cast<LONG>(引数.高さ))
    {
    }
	
	Application::~Application() 
	{
	}

    int Application::Initialize(HWND ウィンドウハンドル)
    {
        パイプラインの読み込み(ウィンドウハンドル, 幅_, 高さ_);
        アセットの読み込み();

        return 0;
    }

    void Application::Finalize()
    {
        // GPUが、デストラクタによってクリーンアップされようとしているリソースを
        // 参照しなくなっていることを確認
        GPUを待つ();

        CloseHandle(フェンスイベント_);
    }

    
    int Application::Update()
    {
        // オブジェクトの動きの更新
        angle += 0.1f;
		if (DirectX::XM_2PI <= angle) angle -= DirectX::XM_2PI;//360 度を超えないようにする

        // シーンのレンダリングに必要なコマンドをすべてコマンドリストに記録
        コマンドリストの記録();

        // コマンドリストを実行
        ID3D12CommandList* pコマンドリスト列[] = { コマンドリスト_.get() };
        コマンドキュー_->ExecuteCommandLists(
            _countof(pコマンドリスト列), pコマンドリスト列);

        // フレームを(画面に)提示する
        winrt::check_hresult(スワップチェーン_->Present(1, 0));

        次のフレームに行く();

        return 0;
    }

    void Application::パイプラインの読み込み(HWND ウィンドウハンドル, 
        unsigned int 幅, unsigned int 高さ)
	{
        UINT dxgiファクトリーフラグ = 0;

#if defined(_DEBUG)
        // デバッグレイヤーを有効にする（グラフィックツールの「オプション機能」が必要）
        // 注意：デバイス作成後にデバッグ・レイヤーを有効にすると、アクティブ・デバイス
        // は無効になる
        {
            winrt::com_ptr<ID3D12Debug> デバッグコントローラー;
            if (SUCCEEDED(D3D12GetDebugInterface(
                __uuidof(デバッグコントローラー), デバッグコントローラー.put_void())))
            {
                デバッグコントローラー->EnableDebugLayer();

                // 追加のデバッグレイヤを有効化
                dxgiファクトリーフラグ |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }
#endif
        // DXGI オブジェクトを生成する
        winrt::com_ptr<IDXGIFactory4> ファクトリー;
        winrt::check_hresult(CreateDXGIFactory2(dxgiファクトリーフラグ, 
            __uuidof(ファクトリー), ファクトリー.put_void()));

		// デバイスを作成
        if (Warpデバイス使用_)
        {
            winrt::com_ptr<IDXGIAdapter> WARPアダプター;
            winrt::check_hresult(ファクトリー->EnumWarpAdapter(
                __uuidof(WARPアダプター), WARPアダプター.put_void()));

            winrt::check_hresult(D3D12CreateDevice(
                WARPアダプター.get(),
                D3D_FEATURE_LEVEL_12_0,// DX12の機能レベルに決め打ち
                __uuidof(デバイス_), デバイス_.put_void()
            ));
        }
        else
        {
            winrt::com_ptr<IDXGIAdapter1> ハードウェアアダプター;
            ハードウェアアダプターの取得(ファクトリー.get(), 
                ハードウェアアダプター.put());

            winrt::check_hresult(D3D12CreateDevice(
                ハードウェアアダプター.get(),
                D3D_FEATURE_LEVEL_12_0,// DX12の機能レベルに決め打ち
                __uuidof(デバイス_), デバイス_.put_void()
            ));
        }

        // コマンドキューについて記述し、作成する。
        D3D12_COMMAND_QUEUE_DESC キュー記述子 = {};
        キュー記述子.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        キュー記述子.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        winrt::check_hresult(デバイス_->CreateCommandQueue(&キュー記述子, 
            __uuidof(コマンドキュー_), コマンドキュー_.put_void()));

        // スワップチェーンを記述し、作成する。
        DXGI_SWAP_CHAIN_DESC1 スワップチェーン記述子 = {};
        スワップチェーン記述子.BufferCount = フレームバッファ数;
        スワップチェーン記述子.Width = 幅;
        スワップチェーン記述子.Height = 高さ;
        スワップチェーン記述子.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        スワップチェーン記述子.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        スワップチェーン記述子.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        スワップチェーン記述子.SampleDesc.Count = 1;

        winrt::com_ptr<IDXGISwapChain1> スワップチェーン;
        winrt::check_hresult(ファクトリー->CreateSwapChainForHwnd(
            コマンドキュー_.get(),// スワップチェーンは強制的なフラッシュにキューが必要
            ウィンドウハンドル,
            &スワップチェーン記述子,
            nullptr,
            nullptr,
            スワップチェーン.put()
        ));

        // このアプリケーションはフルスクリーンのトランジションをサポートしていない
        winrt::check_hresult(ファクトリー->MakeWindowAssociation(ウィンドウハンドル, 
            DXGI_MWA_NO_ALT_ENTER));

        スワップチェーン.as(スワップチェーン_);
        バックバッファ番号_ = スワップチェーン_->GetCurrentBackBufferIndex();

        // デスクリプターヒープを作成
        {
            // レンダー・ターゲット・ビュー（RTV）デスクリプターヒープを記述し、作成する。
            D3D12_DESCRIPTOR_HEAP_DESC rtvヒープ記述子 = {};
            rtvヒープ記述子.NumDescriptors = フレームバッファ数;
            rtvヒープ記述子.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvヒープ記述子.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            winrt::check_hresult(デバイス_->CreateDescriptorHeap(&rtvヒープ記述子, 
                __uuidof(rtvヒープ_), rtvヒープ_.put_void()));

            // 深度ステンシルビュー（DSV）デスクリプターヒープを記述し、作成する。
            D3D12_DESCRIPTOR_HEAP_DESC dsvヒープ記述子 = {};
            dsvヒープ記述子.NumDescriptors = 1;
            dsvヒープ記述子.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            dsvヒープ記述子.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            winrt::check_hresult(デバイス_->CreateDescriptorHeap(&dsvヒープ記述子,
                __uuidof(dsvヒープ_), dsvヒープ_.put_void()));

            // オブジェクト用のシェーダー・リソース・ビュー(SRV)ヒープを記述し、作成する。
            D3D12_DESCRIPTOR_HEAP_DESC オブジェクトヒープ記述子 = {};
            オブジェクトヒープ記述子.NumDescriptors = 2;// 定数バッファ2つ
            オブジェクトヒープ記述子.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            オブジェクトヒープ記述子.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            winrt::check_hresult(デバイス_->CreateDescriptorHeap(&オブジェクトヒープ記述子,
                __uuidof(オブジェクトヒープ_), オブジェクトヒープ_.put_void()));

            rtv記述子サイズ_ = デバイス_->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        }

        // フレームリソースを作成
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE rtvハンドル(
                rtvヒープ_->GetCPUDescriptorHandleForHeapStart());

            // フレームごとのRTVを作成
            for (UINT n = 0; n < フレームバッファ数; n++)
            {
                winrt::check_hresult(スワップチェーン_->GetBuffer(n, 
                    __uuidof(レンダーターゲット_[n]), レンダーターゲット_[n].put_void()));
                デバイス_->CreateRenderTargetView(レンダーターゲット_[n].get(), nullptr,
                    rtvハンドル);
                rtvハンドル.Offset(1, rtv記述子サイズ_);

                winrt::check_hresult(デバイス_->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT, 
                    __uuidof(コマンドアロケーター_[n]), コマンドアロケーター_[n].put_void()));
            }
        }
        // 深度バッファと深度ステンシルビューの生成
        {
            D3D12_CLEAR_VALUE 深度クリア値 = {};
            深度クリア値.Format = DXGI_FORMAT_D32_FLOAT;
            深度クリア値.DepthStencil.Depth = 1.0f;
            深度クリア値.DepthStencil.Stencil = 0;

			CD3DX12_HEAP_PROPERTIES 深度ヒーププロパティ(D3D12_HEAP_TYPE_DEFAULT);
			CD3DX12_RESOURCE_DESC 深度リソース記述子 =
                CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, 幅_, 高さ_, 1, 1);
            深度リソース記述子.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            winrt::check_hresult(デバイス_->CreateCommittedResource(
                &深度ヒーププロパティ, D3D12_HEAP_FLAG_NONE,
                &深度リソース記述子, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &深度クリア値,
                __uuidof(深度ステンシル_), 深度ステンシル_.put_void()
            ));
            深度ステンシル_->SetName(L"深度ステンシルリソース");

            D3D12_DEPTH_STENCIL_VIEW_DESC 深度ステンシル記述子 = {};
            深度ステンシル記述子.Format = DXGI_FORMAT_D32_FLOAT;
            深度ステンシル記述子.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            深度ステンシル記述子.Flags = D3D12_DSV_FLAG_NONE;
            デバイス_->CreateDepthStencilView(深度ステンシル_.get(), &深度ステンシル記述子,
                dsvヒープ_->GetCPUDescriptorHandleForHeapStart());
        }
    }

	// ランダムな色のテクスチャデータを生成
    static std::vector<uint32_t> テクスチャデータの生成(int 幅, int 高さ)
    {
        const int テクスチャサイズ = 幅 * 高さ;
		std::vector<uint32_t> データ(テクスチャサイズ);

        std::mt19937 乱数エンジン;
        std::random_device デバイス乱数;
        乱数エンジン.seed(デバイス乱数());
		std::uniform_int_distribution<uint32_t> 一様分布(0, 0xffffff);

        for (int n = 0; n < テクスチャサイズ; n++)
        {
            データ[n] = 0xff000000 + 一様分布(乱数エンジン);
        }

        return データ;
    }

	void Application::アセットの読み込み()
	{
        // ルート署名を作成する。
        {
            D3D12_FEATURE_DATA_ROOT_SIGNATURE 機能データ = {};

			// これは、サポートする最高バージョン。CheckFeatureSupportが成功すると、
            // 返されるHighestVersionはこれより大きくならない
            機能データ.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

            if (FAILED(デバイス_->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, 
                &機能データ, sizeof(機能データ))))
            {
                機能データ.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
            }

            // ■修正：テクスチャのSRVを削除
            CD3DX12_DESCRIPTOR_RANGE1 範囲配列[2];
            範囲配列[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
            範囲配列[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

            CD3DX12_ROOT_PARAMETER1 ルートパラメータ配列[1];
            ルートパラメータ配列[0].InitAsDescriptorTable(_countof(範囲配列),
                &範囲配列[0], D3D12_SHADER_VISIBILITY_VERTEX);

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC ルート署名記述子;
            ルート署名記述子.Init_1_1(_countof(ルートパラメータ配列), ルートパラメータ配列, 
                0, nullptr, 
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


            winrt::com_ptr<ID3DBlob> 署名;
            winrt::com_ptr<ID3DBlob> エラー;
            winrt::check_hresult(D3DX12SerializeVersionedRootSignature(&ルート署名記述子,
                機能データ.HighestVersion, 署名.put(), エラー.put()));
            winrt::check_hresult(デバイス_->CreateRootSignature(0,
                署名->GetBufferPointer(), 署名->GetBufferSize(), 
                __uuidof(ルート署名_), ルート署名_.put_void()));
        }

        // シェーダーのコンパイルと読み込みを含む、パイプライン状態を作成する。
        {
            winrt::com_ptr<ID3DBlob> 頂点シェーダ, ピクセルシェーダ;
            winrt::com_ptr<ID3DBlob> エラー;

            UINT コンパイルフラグ = 0;
#if defined(_DEBUG)
            // グラフィックデバッグツールを使った、より優れたシェーダーデバッグを可能にする
            コンパイルフラグ |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            HRESULT result;
            result = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", 
                コンパイルフラグ, 0, 頂点シェーダ.put(), エラー.put()); if (result) ShowError(result, エラー.get());
            result = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0",
                コンパイルフラグ, 0, ピクセルシェーダ.put(), エラー.put()); if (result) ShowError(result, エラー.get());

            // ■修正：頂点入力レイアウトを定義する。
            D3D12_INPUT_ELEMENT_DESC 入力要素記述子[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,    0, D3D12_APPEND_ALIGNED_ELEMENT,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            };

            // グラフィックスパイプラインステートオブジェクト（PSO）を記述し、作成する。
            D3D12_GRAPHICS_PIPELINE_STATE_DESC pso記述子 = {};
            pso記述子.InputLayout = { 入力要素記述子, _countof(入力要素記述子) };
            pso記述子.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;// 三角形リスト
            pso記述子.pRootSignature = ルート署名_.get();
            pso記述子.VS = CD3DX12_SHADER_BYTECODE(頂点シェーダ.get());
            pso記述子.PS = CD3DX12_SHADER_BYTECODE(ピクセルシェーダ.get());
            pso記述子.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);// ラスタライズ用の設定
			pso記述子.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);// 結果の合成用の設定
            pso記述子.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);// 深度バッファを使う
			pso記述子.SampleMask = UINT_MAX;// サンプルマスクを使わない
			pso記述子.NumRenderTargets = 1;// レンダーターゲットの数
			pso記述子.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;// レンダーターゲットのフォーマット
			pso記述子.DSVFormat = DXGI_FORMAT_D32_FLOAT;// 深度ステンシルのフォーマット
			pso記述子.SampleDesc.Count = 1;// サンプリングの数
            winrt::check_hresult(デバイス_->CreateGraphicsPipelineState(&pso記述子,
                __uuidof(パイプラインステート_), パイプラインステート_.put_void()));
        }

        // コマンドリストを生成
        winrt::check_hresult(デバイス_->CreateCommandList(0, 
            D3D12_COMMAND_LIST_TYPE_DIRECT, 
            コマンドアロケーター_[バックバッファ番号_].get(), 
            パイプラインステート_.get(), 
            __uuidof(コマンドリスト_), コマンドリスト_.put_void()));

        // シーンオブジェクトを作成
		DirectX::XMVECTOR カメラ位置_   = DirectX::XMVectorSet(0.0f, 0.0f, -5.0f, 0.0f);// ■修正：後ろに下げる
        DirectX::XMVECTOR カメラ注視点_ = DirectX::XMVectorSet(0.0f, 0.0f,  0.0f, 0.0f);
        ビュー行列_ = DirectX::XMMatrixLookAtLH(カメラ位置_, カメラ注視点_, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    	射影行列_ = DirectX::XMMatrixPerspectiveFovLH(45.0f / 180.0f * DirectX::XM_PI, アスペクト比_, 0.01f, 100.0f);

        // 頂点バッファを作成
        {
            // ■修正：三角形のジオメトリを定義する
            頂点 頂点列[] =
			{   //    x       y      z       色
                { { +0.25f, +0.25f, 0.0f }, 0xff000000 },// 右上  3--0
				{ { +0.25f, -0.25f, 0.0f }, 0xff00ff00 },// 右下  | /|
                { { -0.25f, -0.25f, 0.0f }, 0xff00ffff },// 左下  |/ |  
                { { -0.25f, +0.25f, 0.0f }, 0xff0000ff } // 左上  2--1
            };
            // ■追加：三角形ポリゴンを組み立てる頂点番号の指定
            uint32_t インデックス列[] =
            {
				0, 1, 2, // 右下三角形
				3, 0, 2  // 左上三角形
            };

            const UINT 頂点バッファサイズ = sizeof(頂点列);

            // 注: アップロード ヒープを使用して頂点バッファーのような静的データを転送するのはお勧めできない
            // GPU が必要とするたびに、アップロード ヒープが集結される。デフォルトのヒープ使用法で読んで欲しい。
            // コードを簡素化するため、また実際に転送する頂点が非常に少ないので、
            // ここではアップロード ヒープが使用されています。
			CD3DX12_RESOURCE_DESC 頂点バッファ記述子 = CD3DX12_RESOURCE_DESC::Buffer(頂点バッファサイズ);
			CD3DX12_HEAP_PROPERTIES ヒーププロパティ(D3D12_HEAP_TYPE_UPLOAD);
            winrt::check_hresult(デバイス_->CreateCommittedResource(
                &ヒーププロパティ,
                D3D12_HEAP_FLAG_NONE,
                &頂点バッファ記述子,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                __uuidof(頂点バッファ_), 頂点バッファ_.put_void()));

            // 三角形データを頂点バッファにコピー
            UINT8* 開始アドレス;
            CD3DX12_RANGE 読み込み範囲(0, 0); // このリソースをCPUから読む気はない
            winrt::check_hresult(頂点バッファ_->Map(0, &読み込み範囲, reinterpret_cast<void**>(&開始アドレス)));
            memcpy(開始アドレス, 頂点列, sizeof(頂点列));
            頂点バッファ_->Unmap(0, nullptr);

            // 頂点バッファビューの初期化
            頂点バッファビュー_.BufferLocation = 頂点バッファ_->GetGPUVirtualAddress();
            頂点バッファビュー_.StrideInBytes = sizeof(頂点);
            頂点バッファビュー_.SizeInBytes = 頂点バッファサイズ;

			// ■追加：インデックスバッファを作成
            const UINT インデックスバッファサイズ = sizeof(インデックス列);

            CD3DX12_RESOURCE_DESC インデックスバッファ記述子 = CD3DX12_RESOURCE_DESC::Buffer(インデックスバッファサイズ);
            winrt::check_hresult(デバイス_->CreateCommittedResource(
				&ヒーププロパティ,// 頂点バッファのヒーププロパティを流用
                D3D12_HEAP_FLAG_NONE,
                &インデックスバッファ記述子,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                __uuidof(インデックスバッファ_), インデックスバッファ_.put_void()));

            // インデックスデータをインデックスバッファにコピー
            winrt::check_hresult(インデックスバッファ_->Map(0, &読み込み範囲, reinterpret_cast<void**>(&開始アドレス)));
            memcpy(開始アドレス, インデックス列, sizeof(インデックス列));
            インデックスバッファ_->Unmap(0, nullptr);

            // ■追加：インデックスバッファビューの初期化
            インデックスバッファビュー_.BufferLocation = インデックスバッファ_->GetGPUVirtualAddress();
            インデックスバッファビュー_.Format = DXGI_FORMAT_R32_UINT;
            インデックスバッファビュー_.SizeInBytes = インデックスバッファサイズ;
        }

        // 注意：ComPtrはCPUオブジェクトだが、このリソースは、それを参照するコマンドリストが
        // GPU上で実行を終了するまで、スコープ内にとどまる必要がある。リソースが早期に破壊
        // されないように、このメソッドの最後でGPUをフラッシュする。

        D3D12_CPU_DESCRIPTOR_HANDLE オブジェクトヒープハンドル = オブジェクトヒープ_->GetCPUDescriptorHandleForHeapStart();
        UINT cbvヒープサイズ = デバイス_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);// ■修正

        // 定数バッファの生成
        {
            // ■修正：シーン定数バッファ
            const UINT シーン定数バッファデータサイズ = sizeof(シーン定数バッファ);

			CD3DX12_HEAP_PROPERTIES シーンヒーププロパティ(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC シーンリソース記述子 = CD3DX12_RESOURCE_DESC::Buffer(シーン定数バッファデータサイズ);
            winrt::check_hresult(デバイス_->CreateCommittedResource(
                &シーンヒーププロパティ, D3D12_HEAP_FLAG_NONE,
                &シーンリソース記述子, D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                __uuidof(シーン定数バッファ_), シーン定数バッファ_.put_void()));
            シーン定数バッファ_->SetName(L"シーン定数バッファ");

            // シーン定数バッファをマッピングし、初期化する。
            // 今回は、シーン変数は固定なので、すぐにマッピングを解除する。
            CD3DX12_RANGE シーン読み込み範囲(0, 0);        // CPU上でこのリソースから読み出すつもりはない
            シーン定数バッファ *マップされたシーン定数バッファ;
            winrt::check_hresult(シーン定数バッファ_->Map(0, &シーン読み込み範囲,
                reinterpret_cast<void**>(&マップされたシーン定数バッファ)));

            // 定数バッファの初期化
            シーン定数バッファ シーン初期化データ;
            DirectX::XMStoreFloat4x4(&(シーン初期化データ.ビュー射影行列), XMMatrixTranspose(ビュー行列_* 射影行列_));
            memcpy(マップされたシーン定数バッファ, &シーン初期化データ, シーン定数バッファデータサイズ);
			シーン定数バッファ_->Unmap(0, nullptr);// マッピングを解除する

            // 定数バッファビューの作成
            D3D12_CONSTANT_BUFFER_VIEW_DESC シーンcbv記述子 = {};
            シーンcbv記述子.BufferLocation = シーン定数バッファ_->GetGPUVirtualAddress();
            シーンcbv記述子.SizeInBytes = static_cast<UINT>(シーン定数バッファ_->GetDesc().Width);
            デバイス_->CreateConstantBufferView(&シーンcbv記述子, オブジェクトヒープハンドル);

            // ■追加：オブジェクト定数バッファ
            const UINT オブジェクト定数バッファデータサイズ = sizeof(オブジェクト定数バッファ);

            CD3DX12_HEAP_PROPERTIES オブジェクトヒーププロパティ(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC オブジェクトリソース記述子 = CD3DX12_RESOURCE_DESC::Buffer(オブジェクト定数バッファデータサイズ);
            winrt::check_hresult(デバイス_->CreateCommittedResource(
                &オブジェクトヒーププロパティ, D3D12_HEAP_FLAG_NONE,
                &オブジェクトリソース記述子, D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                __uuidof(オブジェクト定数バッファ_), オブジェクト定数バッファ_.put_void()));
            オブジェクト定数バッファ_->SetName(L"オブジェクト定数バッファ");

            // 定数バッファをマッピングし、初期化する。アプリが終了するまで、このマッピングは解除しない。
            // リソースの寿命が尽きるまでマッピングされたままにしておいても問題ない。
            CD3DX12_RANGE オブジェクト読み込み範囲(0, 0);        // CPU上でこのリソースから読み出すつもりはない
            winrt::check_hresult(オブジェクト定数バッファ_->Map(0, &オブジェクト読み込み範囲,
                reinterpret_cast<void**>(&マップされたオブジェクト定数バッファ_)));

            // 定数バッファの初期化
            オブジェクト定数バッファ 初期化データ;
            for(int i = 0; i < インスタンス数; i++)
                DirectX::XMStoreFloat4x4(初期化データ.ワールド行列 + i, DirectX::XMMatrixIdentity());
            memcpy(マップされたオブジェクト定数バッファ_, &初期化データ, オブジェクト定数バッファデータサイズ);

            // 定数バッファビューの作成
            オブジェクトヒープハンドル.ptr += cbvヒープサイズ;// シーンのCBVの次の位置に置く
            D3D12_CONSTANT_BUFFER_VIEW_DESC オブジェクトcbv記述子 = {};
            オブジェクトcbv記述子.BufferLocation = オブジェクト定数バッファ_->GetGPUVirtualAddress();
            オブジェクトcbv記述子.SizeInBytes = static_cast<UINT>(オブジェクト定数バッファ_->GetDesc().Width);
            デバイス_->CreateConstantBufferView(&オブジェクトcbv記述子, オブジェクトヒープハンドル);
        }

        // 最初のGPUセットアップを始めるためにコマンドリストを閉じて実行する
        winrt::check_hresult(コマンドリスト_->Close());
        ID3D12CommandList* コマンドリスト列[] = { コマンドリスト_.get() };
        コマンドキュー_->ExecuteCommandLists(_countof(コマンドリスト列), コマンドリスト列);

        // 同期オブジェクトを作成し、アセットがGPUにアップロードされるまで待つ。
        {
            winrt::check_hresult(デバイス_->CreateFence(
                フェンス値_[バックバッファ番号_], D3D12_FENCE_FLAG_NONE, 
                __uuidof(フェンス_), フェンス_.put_void()));
            フェンス値_[バックバッファ番号_]++;

            // フレーム同期に使用するイベントハンドルを作成する
            フェンスイベント_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (フェンスイベント_ == nullptr)
            {
                winrt::check_hresult(HRESULT_FROM_WIN32(GetLastError()));
            }

            // コマンドリストの実行を待つ。メインループで同じコマンドリストを再利用して
            // おり、今はセットアップが完了するのを待ってから続けたい。
            GPUを待つ();
        }
    }


    void Application::コマンドリストの記録()
    {
       // コマンド・リスト・アロケータは、関連するコマンド・リストがGPUでの
       // 実行を終了したときにのみリセットすることができる。
       // アプリはGPU実行の進捗の判断にフェンスを使うべき。
        winrt::check_hresult(コマンドアロケーター_[バックバッファ番号_]->Reset());

        // コマンド・リストでExecuteCommandList()を呼び出すと、
        // そのコマンド・リストはいつでもリセットすることができるが、
        // 再記録の前にリセットしなければならない。
        winrt::check_hresult(コマンドリスト_->Reset(
            コマンドアロケーター_[バックバッファ番号_].get(), 
            パイプラインステート_.get()));

        // バックバッファをレンダーターゲットとして使用する
        ID3D12Resource* レンダーターゲット = レンダーターゲット_[バックバッファ番号_].get();
        CD3DX12_RESOURCE_BARRIER バリアPresent2RT(CD3DX12_RESOURCE_BARRIER::Transition(
            レンダーターゲット,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
        コマンドリスト_->ResourceBarrier(1, &バリアPresent2RT);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvハンドル(
            rtvヒープ_->GetCPUDescriptorHandleForHeapStart(), バックバッファ番号_, rtv記述子サイズ_);
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvハンドル(
            dsvヒープ_->GetCPUDescriptorHandleForHeapStart());
        コマンドリスト_->OMSetRenderTargets(1, &rtvハンドル, FALSE, &dsvハンドル);

        // コマンドを記録
        const float 背景色[] = { 0.0f, 0.2f, 0.4f, 1.0f };// 赤、緑、青、アルファ
        コマンドリスト_->ClearRenderTargetView(rtvハンドル, 背景色, 0, nullptr);
        コマンドリスト_->ClearDepthStencilView(dsvハンドル, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        // 描画に必要な状態を設定
        コマンドリスト_->SetGraphicsRootSignature(ルート署名_.get());
        コマンドリスト_->RSSetViewports(1, &ビューポート_);
        コマンドリスト_->RSSetScissorRects(1, &はさみ矩形_);

		// ■修正：定数バッファをバインド
        ID3D12DescriptorHeap* ppHeaps[] = { オブジェクトヒープ_.get() };
        auto ヒープハンドル = オブジェクトヒープ_->GetGPUDescriptorHandleForHeapStart();
        コマンドリスト_->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		コマンドリスト_->SetGraphicsRootDescriptorTable(0, ヒープハンドル);

        // ■修正：ポリゴンの描画
        for (int x = 0; x < 3; x++)
            for (int y = 0; y < 3; y++)
                for (int z = 0; z < 3; z++) {
                    DirectX::XMMATRIX x回転行列 = DirectX::XMMatrixRotationX(angle * (float)x);
                    DirectX::XMMATRIX y回転行列 = DirectX::XMMatrixRotationY(angle * (float)y);
                    DirectX::XMMATRIX z回転行列 = DirectX::XMMatrixRotationZ(angle * (float)z);
					DirectX::XMMATRIX 移動行列 = DirectX::XMMatrixTranslation(
						(float)(x - 1) * 1.0f, (float)(y - 1) * 1.0f, (float)(z - 1) * 1.0f);
                    DirectX::XMMATRIX ワールド行列 = z回転行列 * x回転行列 * y回転行列 * 移動行列;// なんとなくUnityの回転順

                    // シェーダは列優先として行列を作用させるがDirectXMathは行優先を想定して計算するので転置して渡す
                    ワールド行列 = XMMatrixTranspose(ワールド行列);
                    int idx = (x * 3 + y) * 3 + z;
                    DirectX::XMStoreFloat4x4(&マップされたオブジェクト定数バッファ_->ワールド行列[idx], ワールド行列);
                }

        コマンドリスト_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        コマンドリスト_->IASetVertexBuffers(0, 1, &頂点バッファビュー_);
		コマンドリスト_->IASetIndexBuffer(&インデックスバッファビュー_);
		コマンドリスト_->DrawIndexedInstanced(6, インスタンス数, 0, 0, 0);// ■修正：6頂点

        // バックバッファは画面更新に使用される
		CD3DX12_RESOURCE_BARRIER バリアRT2Present(CD3DX12_RESOURCE_BARRIER::Transition(
            レンダーターゲット,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        コマンドリスト_->ResourceBarrier(1, &バリアRT2Present);

        winrt::check_hresult(コマンドリスト_->Close());
    }

    void Application::GPUを待つ()
    {
        // キューにシグナルコマンドをスケジュールする
        winrt::check_hresult(コマンドキュー_->Signal(フェンス_.get(), 
            フェンス値_[バックバッファ番号_]));

        // フェンスの処理が終わるまで待つ
        winrt::check_hresult(フェンス_->SetEventOnCompletion(
            フェンス値_[バックバッファ番号_], フェンスイベント_));
        WaitForSingleObjectEx(フェンスイベント_, INFINITE, FALSE);

        // 現在のフレームのフェンス値を増加させる
        フェンス値_[バックバッファ番号_]++;
    }

    // 次のフレームのレンダリングを準備
    void Application::次のフレームに行く()
    {
        // キューにシグナルコマンドをスケジュールする
        const UINT64 現在のフェンス値 = フェンス値_[バックバッファ番号_];
        winrt::check_hresult(コマンドキュー_->Signal(フェンス_.get(), 現在のフェンス値));

        // フレームインデックスを更新
        バックバッファ番号_ = スワップチェーン_->GetCurrentBackBufferIndex();

        // 次のフレームをレンダリングする準備がまだできていない場合は、
        // 準備ができるまで待つ。
        if (フェンス_->GetCompletedValue() < フェンス値_[バックバッファ番号_])
        {
            winrt::check_hresult(フェンス_->SetEventOnCompletion(
                フェンス値_[バックバッファ番号_], フェンスイベント_));
            WaitForSingleObjectEx(フェンスイベント_, INFINITE, FALSE);
        }

        // 次のフレームのフェンス値を設定する
        フェンス値_[バックバッファ番号_] = 現在のフェンス値 + 1;
    }

    void Application::ハードウェアアダプターの取得(
        _In_ IDXGIFactory1* pファクトリー,
        _Outptr_result_maybenull_ IDXGIAdapter1** ppアダプター,
        bool 高パフォーマンスアダプターの要求)
    {
        *ppアダプター = nullptr;

        winrt::com_ptr<IDXGIAdapter1> アダプター;

        winrt::com_ptr<IDXGIFactory6> ファクトリー6;
        if (winrt::check_hresult(pファクトリー->QueryInterface(
            __uuidof(ファクトリー6), ファクトリー6.put_void())))
        {
            for (
                UINT アダプター番号 = 0;
                winrt::check_hresult(ファクトリー6->EnumAdapterByGpuPreference(
                    アダプター番号,
                    高パフォーマンスアダプターの要求 == true ? 
                        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : 
                        DXGI_GPU_PREFERENCE_UNSPECIFIED,
                    __uuidof(アダプター), アダプター.put_void()));
                    ++アダプター番号)
            {
                DXGI_ADAPTER_DESC1 記述子;
                アダプター->GetDesc1(&記述子);

                if (記述子.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Basic Render Driverアダプタは選択しないでください。
                    // ソフトウェア・アダプタが必要な場合は、コマンドラインに「/warp 」
                    // と入力してください。
                    continue;
                }

                // アダプタがDirect3D 12をサポートしているかどうかを確認するが、
                // 実際のデバイスはまだ作成しない
                if (winrt::check_hresult(D3D12CreateDevice(アダプター.get(),
                    D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        if (アダプター.get() == nullptr)
        {
            for (UINT アダプター番号 = 0; winrt::check_hresult(
                pファクトリー->EnumAdapters1(アダプター番号, アダプター.put()));
                ++アダプター番号)
            {
                DXGI_ADAPTER_DESC1 記述子;
                アダプター->GetDesc1(&記述子);

                if (記述子.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Basic Render Driverアダプタは選択しないでください。
                    // ソフトウェア・アダプタが必要な場合は、コマンドラインに「/warp 」
                    // と入力してください。
                    continue;
                }

                // アダプタがDirect3D 12をサポートしているかどうかを確認するが、
                // 実際のデバイスはまだ作成しない
                if (winrt::check_hresult(D3D12CreateDevice(アダプター.get(),
                    D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        *ppアダプター = アダプター.detach();
    }

    void Application::ShowError(HRESULT result, ID3DBlob* error)
    {
        if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            ::OutputDebugString(L"ファイルが見当たりません\n");
            exit(1);
        }

        std::string s((char*)error->GetBufferPointer(), error->GetBufferSize());
        s += "\n";
        OutputDebugStringA(s.c_str());

        exit(1);
    }
}
