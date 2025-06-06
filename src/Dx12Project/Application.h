﻿#pragma once
#include "framework.h"

namespace tpot
{
	struct ApplicationArgs
	{
		unsigned int 幅 = 1920;
		unsigned int 高さ = 1080;
		bool useWarpDevice = false;
	};

	class Application
	{
	public:
		Application(const ApplicationArgs& args);
		~Application();

		int Initialize(HWND hwnd);
		void Finalize();

		int Update();

	private:
		//  このアプリケーションでは、フレームバッファ数 の意味を一度に GPU にキューイングされる
		//  フレームの最大数とDXGI スワップチェインのバックバッファの数の両方を意味します。大半は、
		//  これは便利でうまく機能します。しかし、アプリケーションが利用可能なバックバッファ数よりも
		//  多くのフレームをキューに入れたい場合もあるでしょう。ユーザー入力に依存するフレームを過度に
		//  バッファリングすると、アプリケーションに顕著なレイテンシが発生しうることに
		//  注意する必要があります。
		static const UINT フレームバッファ数 = 2;

		struct 頂点
		{
			DirectX::XMFLOAT3 位置;
			uint32_t 色;// ■修正
		};

		// 定数バッファの定義
		struct シーン定数バッファ
		{
			DirectX::XMFLOAT4X4 ビュー射影行列;

			// 定数バッファは256バイト・アライメント。構造体にパディングを追加して、
			// 複数のバッファを配列インデックスにできるようにする。
			char padding[256 - 64];
		};

		// ■追加：ワールド行列配列を導入
		static const UINT インスタンス数 = 3 * 3 * 3;
		struct オブジェクト定数バッファ
		{
			DirectX::XMFLOAT4X4 ワールド行列[インスタンス数];

			// 定数バッファは256バイト・アライメント。構造体にパディングを追加して、
			// 複数のバッファを配列インデックスにできるようにする。
			char padding[256*((インスタンス数+3)/4) - 64*インスタンス数];
		};

		// ビューポート
		UINT 幅_ = 1920;
		UINT 高さ_ = 1080;
		float アスペクト比_ = 1920.f/ 1080.f;

		// アダプター情報
		// Windows Advanced Rasterization Platform(高速で完全に準拠したソフトウェアラスタライザー)の使用
		bool Warpデバイス使用_ = false;

		void ハードウェアアダプターの取得(
			_In_ IDXGIFactory1* pファクトリー,
			_Outptr_result_maybenull_ IDXGIAdapter1** ppアダプター,
			bool 高パフォーマンスアダプターの要求 = false);

		// パイプラインオブジェクト
		CD3DX12_VIEWPORT ビューポート_;
		CD3DX12_RECT はさみ矩形_;		
		winrt::com_ptr<IDXGISwapChain3> スワップチェーン_ = nullptr;
		winrt::com_ptr<ID3D12Device> デバイス_ = nullptr;
		winrt::com_ptr<ID3D12CommandQueue> コマンドキュー_ = nullptr;
		winrt::com_ptr<ID3D12GraphicsCommandList> コマンドリスト_ = nullptr;
		winrt::com_ptr<ID3D12RootSignature> ルート署名_ = nullptr;
		winrt::com_ptr<ID3D12PipelineState> パイプラインステート_ = nullptr;
		winrt::com_ptr<ID3D12CommandAllocator> コマンドアロケーター_[フレームバッファ数] = {};
		winrt::com_ptr<ID3D12Resource> レンダーターゲット_[フレームバッファ数] = {};
		winrt::com_ptr<ID3D12Resource> 深度ステンシル_;
		winrt::com_ptr<ID3D12DescriptorHeap> rtvヒープ_ = nullptr;
		winrt::com_ptr<ID3D12DescriptorHeap> dsvヒープ_ = nullptr;
		winrt::com_ptr<ID3D12DescriptorHeap> オブジェクトヒープ_ = nullptr;
		UINT rtv記述子サイズ_ = 0;

		// シーンリソース
		DirectX::XMMATRIX ビュー行列_;
		DirectX::XMMATRIX 射影行列_;

		// オブジェクトリソース
		float angle = 0.0f;
		winrt::com_ptr<ID3D12Resource> 頂点バッファ_;
		winrt::com_ptr<ID3D12Resource> インデックスバッファ_;		// ■追加
		D3D12_VERTEX_BUFFER_VIEW 頂点バッファビュー_;
		D3D12_INDEX_BUFFER_VIEW インデックスバッファビュー_;		// ■追加
		winrt::com_ptr<ID3D12Resource> シーン定数バッファ_;			// ■追加
		winrt::com_ptr<ID3D12Resource> オブジェクト定数バッファ_;	// ■追加
		オブジェクト定数バッファ *マップされたオブジェクト定数バッファ_;

		// 同期オブジェクト
		UINT バックバッファ番号_ = 0;
		HANDLE フェンスイベント_ = nullptr;
		winrt::com_ptr<ID3D12Fence> フェンス_ = nullptr;
		UINT64 フェンス値_[フレームバッファ数] = {};

		// Initialize 用
		void パイプラインの読み込み(HWND ウィンドウハンドル, unsigned int 幅, unsigned int 高さ);
		void アセットの読み込み();

		// 同期用
		void GPUを待つ();
		void 次のフレームに行く();

		// Update用
		void コマンドリストの記録();

		// エラー表示
		void ShowError(HRESULT result, ID3DBlob* errorBlob);
	};
}