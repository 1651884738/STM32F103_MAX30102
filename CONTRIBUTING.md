# 贡献指南

感谢你考虑为 STM32-MAX30102 项目做出贡献！

## 如何贡献

### 报告 Bug

如果你发现了 Bug，请[创建一个 Issue](https://github.com/your-username/STM32-MAX30102/issues)，并包含以下信息：

- Bug 的清晰描述
- 重现步骤
- 预期行为
- 实际行为
- 硬件配置（MCU型号、传感器型号等）
- 软件版本（IDE、工具链版本）
- 错误日志或截图

### 建议新功能

如果你有新功能建议：

1. 先查看[现有 Issues](https://github.com/your-username/STM32-MAX30102/issues)，避免重复
2. 创建一个新 Issue，标签选择 "enhancement"
3. 详细描述功能需求和应用场景
4. 如果可能，提供设计思路或伪代码

### 提交代码

#### 1. Fork 和克隆

```bash
# Fork 项目到你的账号
# 然后克隆
git clone https://github.com/your-username/STM32-MAX30102.git
cd STM32-MAX30102
```

#### 2. 创建分支

```bash
git checkout -b feature/your-feature-name
# 或
git checkout -b bugfix/issue-number
```

#### 3. 编码规范

**C 代码风格：**
- 使用 4 空格缩进（不使用 Tab）
- 函数命名：`Module_FunctionName()` (如 `PPG_Filter_Process()`)
- 变量命名：`snake_case` (如 `heart_rate`, `sample_counter`)
- 常量命名：`UPPER_CASE` (如 `WAVE_WIDTH`, `HR_BUFFER_SIZE`)
- 注释：使用 Doxygen 风格

```c
/**
 * @brief 函数简要说明
 * @param param1 参数1说明
 * @param param2 参数2说明
 * @return 返回值说明
 */
float Function_Name(float param1, uint32_t param2);
```

**文件组织：**
- 头文件保护：`#ifndef MODULE_H` / `#define MODULE_H` / `#endif`
- 包含顺序：系统头文件 → HAL库 → 项目头文件
- 每个模块独立文件（.h + .c）

#### 4. 提交规范

提交信息格式：

```
<type>(<scope>): <subject>

<body>

<footer>
```

**类型（type）：**
- `feat`: 新功能
- `fix`: Bug 修复
- `docs`: 文档更新
- `style`: 代码格式（不影响功能）
- `refactor`: 重构
- `perf`: 性能优化
- `test`: 测试相关
- `chore`: 构建/工具配置

**示例：**

```bash
git commit -m "feat(algorithm): add adaptive threshold for peak detection

- Implemented dynamic threshold calculation based on signal variance
- Improved accuracy by 15% in low signal conditions
- Tested with 50 samples

Closes #42"
```

#### 5. 推送和 PR

```bash
# 推送到你的 fork
git push origin feature/your-feature-name

# 然后在 GitHub 上创建 Pull Request
```

**Pull Request 描述应包含：**
- 改动的目的和动机
- 实现方法的简要说明
- 测试情况
- 相关 Issue 编号

### 代码审查流程

1. 自动化检查（CI）通过
2. 至少一名维护者审查
3. 解决所有评论
4. 合并到主分支

## 开发环境设置

### 必需工具

- **IDE**: STM32CubeIDE 1.8+ 或 Keil MDK 5
- **编译器**: ARM GCC 10+
- **调试器**: ST-Link V2
- **Git**: 2.30+

### 推荐工具

- **格式化**: clang-format
- **静态分析**: cppcheck
- **文档生成**: Doxygen

### 构建测试

```bash
# CMake 构建
mkdir build && cd build
cmake ..
make

# 单元测试（如果可用）
make test
```

## 文档贡献

文档同样重要！欢迎改进：

- README.md
- docs/algorithm.md
- docs/hardware.md
- 代码注释

### 文档风格

- 使用 Markdown 格式
- 中英文混排时，英文前后加空格
- 代码块标注语言类型
- 添加目录和锚点链接

## 测试要求

### 功能测试

新功能必须经过以下测试：

1. **单元测试**：核心算法函数
2. **集成测试**：完整流程
3. **硬件测试**：实际硬件验证

### 性能测试

对性能敏感的改动需要提供：

- CPU占用率测试结果
- 内存占用分析
- 响应时间测量

### 测试报告模板

```markdown
## 测试环境
- MCU: STM32F103C8T6
- 传感器: MAX30102
- 测试样本: 50次测量

## 测试结果
- 准确率: 95%
- 响应时间: 2.3s
- CPU占用: 28%

## 已知问题
- 无
```

## 行为准则

### 我们的承诺

为了营造开放友好的环境，我们承诺：

- 尊重不同观点和经验
- 接受建设性批评
- 专注于对社区最有利的事
- 对他人表现同理心

### 不可接受的行为

- 使用性暗示语言或图像
- 人身攻击或侮辱性评论
- 公开或私下骚扰
- 未经许可发布他人隐私信息
- 其他不专业或不受欢迎的行为

## 许可证

提交贡献即表示你同意将代码按照项目的 [MIT License](LICENSE) 发布。

## 联系方式

- 项目维护者：your.email@example.com
- Issue 讨论：[GitHub Issues](https://github.com/your-username/STM32-MAX30102/issues)
- 技术交流：[GitHub Discussions](https://github.com/your-username/STM32-MAX30102/discussions)

## 致谢

感谢所有贡献者！你们的努力让这个项目变得更好。

---

**快速开始贡献：**
1. Fork 项目
2. 创建分支
3. 提交改动
4. 推送分支
5. 创建 Pull Request

期待你的贡献！🎉
