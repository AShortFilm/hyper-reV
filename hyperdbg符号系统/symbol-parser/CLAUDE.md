# symbol-parser - 符号解析模块

**[根目录](../CLAUDE.md) > symbol-parser**

---

## 模块职责

symbol-parser 负责解析 PDB（Program Database）符号文件，提供：

- **PDB 文件加载** - 解析 Microsoft 符号格式
- **符号查询** - 按名称或地址查找符号
- **类型信息提取** - 结构体、枚举、函数签名
- **地址转换** - 符号名 ↔ 地址映射

---

## 关键依赖

### 外部依赖
- **pdbex** (`dependencies/pdbex/`) - PDB 解析库

### CMake 配置
```cmake
add_library(symbol-parser SHARED ${SourceFiles})
target_link_libraries(symbol-parser pdbex)
```

---

## 主要接口

### 符号加载
```cpp
// 加载模块符号
BOOLEAN SymbolLoadModuleSymbols(
    const char* ModuleName,
    UINT64      BaseAddress
);

// 从符号服务器下载
BOOLEAN SymbolDownloadFromServer(
    const char* GuidAndAge,
    const char* SymbolPath
);
```

### 符号查询
```cpp
// 按名称查找符号地址
UINT64 SymbolGetAddressByName(
    const char* SymbolName
);

// 按地址查找符号名称
const char* SymbolGetNameByAddress(
    UINT64 Address
);
```

---

## 代码文件

- `code/symbol-parser.cpp` - 主要解析逻辑
- `code/common-utils.cpp` - 工具函数
- `code/casting.cpp` - 类型转换

---

**相关**: [libhyperdbg 模块](../libhyperdbg/CLAUDE.md)
