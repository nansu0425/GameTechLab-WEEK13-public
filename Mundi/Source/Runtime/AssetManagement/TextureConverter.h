/**
 * @file TextureConverter.h
 * @brief Utility for converting textures to DDS format with caching support
 *
 * This class provides functionality to convert source texture files (PNG, JPG, TGA, etc.)
 * to DDS format using DirectXTex library. It implements a cache system similar to
 * OBJ binary caching to improve texture loading performance.
 */

#pragma once
#include "UEContainer.h"
#include <d3d11.h>
#include <filesystem>

/**
 * @class FTextureConverter
 * @brief Static utility class for texture format conversion and cache management
 */
class FTextureConverter
{
public:
	/**
	 * @brief Convert source texture to DDS format
	 * @param SourcePath Original texture file path (.png, .jpg, .tga, etc.)
	 * @param OutputPath Output DDS file path (auto-generated if empty)
	 * @param Format Target DXGI format (default: BC3_UNORM for fast compression)
	 * @return true if conversion succeeded, false otherwise
	 */
	static bool ConvertToDDS(
		const FString& SourcePath,
		const FString& OutputPath = "",
		DXGI_FORMAT Format = DXGI_FORMAT_BC3_UNORM
	);

	/**
	 * @brief Check if DDS cache needs regeneration
	 * @param SourcePath Original texture file path
	 * @param DDSPath Cached DDS file path
	 * @return true if cache is invalid or missing
	 */
	static bool ShouldRegenerateDDS(
		const FString& SourcePath,
		const FString& DDSPath
	);

	/**
	 * @brief Get DDS cache path for a given source texture
	 * @param SourcePath Original texture file path
	 * @return Generated cache path in Data/TextureCache/
	 */
	static FString GetDDSCachePath(const FString& SourcePath);

	/**
	 * @brief Check if a file extension is supported for WIC loading
	 * @param Extension File extension (including dot, e.g., ".png")
	 * @return true if format is supported
	 */
	static bool IsSupportedFormat(const FString& Extension);

	/**
	 * @brief Generate mipmaps for texture conversion
	 * @param bGenerateMips Whether to generate mipmaps (default: true)
	 */
	static void SetGenerateMipmaps(bool bGenerateMips);

private:
	// Disable instantiation
	FTextureConverter() = delete;
	~FTextureConverter() = delete;

	/**
	 * @brief Get recommended compression format based on image characteristics
	 * @param bHasAlpha Whether the image has alpha channel
	 * @return Optimal DXGI format
	 */
	static DXGI_FORMAT GetRecommendedFormat(bool bHasAlpha);

	/**
	 * @brief Ensure cache directory exists
	 * @param CachePath Path to cache file
	 */
	static void EnsureCacheDirectoryExists(const FString& CachePath);

	// Configuration
	static inline bool bShouldGenerateMipmaps = true;
};
