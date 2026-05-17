# mine.math

职责：提供几何、向量、矩阵、颜色与二维变换数学基础。

当前实现：

- `Vec2` / `Vec3` / `Vec4`：浮点向量，支持算术、点积、叉积与归一化
- `Mat3` / `Mat4`：浮点矩阵，支持平移/缩放/旋转、矩阵乘法与点/向量变换
- `Point` / `Size` / `Rect` / `RoundedRect`：二维几何基础类型
- `Color`：线性空间 RGBA 颜色，支持 8 位打包值转换、预乘与 Alpha 混合
- `Transform2D`：二维仿射变换包装，支持组合、逆变换与矩形映射
- `Math.h`：伞形头文件

验证状态：`xmake build mine.math.test` 与 `xmake run mine.math.test` 已通过，
当前结果为 25 个测试用例、68 个断言全部成功。