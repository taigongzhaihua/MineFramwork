# mine.gfx.d3d11 模块
# Direct3D 11 后端实现

if(WIN32)
    mine_module("mine.gfx.d3d11"
        DEPENDENCIES mine.gfx.rhi mine.diag
    )
    
    # 链接 D3D11 库
    target_link_libraries(mine.gfx.d3d11 PRIVATE
        d3d11
        dxgi
        d3dcompiler
    )
endif()
