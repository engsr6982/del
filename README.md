# DEL - Data Expression Language 数据表达式语言

DEL 是一种数据表达式语言，用于不同格式 JSON 数据之间的转换，并提供简易的数据操作功能。

DEL 本质是基于 JSON Pointer + JSON 文件 (DEL 称之为模板) 实现的一种单行表达式，表达式的运算结果即目标 key 的值。

## 模板

模板是 DEL 的核心，它是一个标准的 JSON 文件，它的核心哲学是：**键（Key）负责寻址目标构造路径，值（Value）负责基于源数据进行计算。**

- **Key (标准 JSON Pointer)**：声明了该条结果在**最终输出 JSON** 中的确切层级与挂载点。解析器将按照 Key 自动构建不存在的嵌套树。
- **Value (DEL 单行表达式)**：一段无副作用的纯计算语句。

```json
{
  "/schema_version": "32",
  "/mLandContext/mOwner": "@/old_data/uuid |> remove_char('-')",
  "/mLandContext/mFlags/isValid": "is_valid($/mLandContext/mOwner) ? true : false"
}
```

_执行顺序约束：C++ 引擎应按照 JSON 模板解析出的键值对顺序依次执行计算并挂载结果。_

---

## 词法规范

### 1. 数据指针

- **源指针 `@/` (Source Pointer)**：只读。用于从原始旧 JSON 中提取数据。
- _语法_：`@/` 后接符合 RFC 6901 的路径字符串。
- _示例_：`@/meta/old_uuid`

- **目标指针 `$/` (Target Pointer)**：状态探针。用于从**已经计算完并挂载**的目标 JSON 缓存区中读取数据，实现跨字段联动。
- _语法_：`$/` 后接符合 RFC 6901 的路径字符串。
- _示例_：`$/mLandContext/mOwner`

### 2. 字面量

- **String**：必须使用双引号或单引号 (`""` / `''`) 包裹，支持内嵌转义符 `\"`。
- **Number**：支持整数和浮点数。
- **Boolean**：保留字 `true` 和 `false`。
- **Null**：保留字 `null`。注意：**当 `@/` 或 `$/` 路径在对应 JSON 中找不到目标时，默认静默求值为 `null`，不抛出异常。**

### 3. 运算符

- **管道**：`|>`
  - 管道符会自动将其**左侧表达式的求值结果**，作为**右侧函数的第一个隐式参数**注入。
  - 写法：`@/raw_str |> remove_char('-') |> to_lower()`
  - 底层等价于：`to_lower(remove_char(@/raw_str, '-'))`
- **控制流**：`?`, `:`, `??`
- **逻辑运算**：`&&`, `||`, `!`
- **关系运算**：`==`, `!=`, `>`, `<`, `>=`, `<=`
- **Lambda 绑定**：`->`
  - 通常用于在一个回调函数中，将一个操作值绑定到一个变量上。
  - 例如: `map(@/foo/list, element -> element |> to_lower())`
  - 其 `element` 指向 `@/foo/list` 中的每一个元素(迭代)。
  - 注意: `->` 运算符并**不具备独立的求值能力**。
- **闭包标点**：`(`, `)`, `,`
- **空白符**：空格、制表符在切词阶段全部被无视丢弃。

---

## 语法优先级

为避免复杂的嵌套导致语法树爆炸，C++ 解析端使用 **Pratt Parser**。

| 优先级等级    | 操作符               | 类型          | 结合性 | 说明                                           |
| :------------ | :------------------- | :------------ | :----- | :--------------------------------------------- |
| **9 (最高)**  | `()`                 | 分组/函数调用 | 无     | 括号提权及 `func(arg1)` 表达式调用             |
| **8**         | `!`, `-`             | 一元前缀      | 右结合 | 逻辑非、负号一元运算                           |
| **7**         | `*`, `/`             | 二元中缀      | 左结合 | 算术乘法、除法                                 |
| **6**         | `+`, `-`             | 二元中缀      | 左结合 | 算术加法、减法                                 |
| **5**         | `\|>`                | 二元中缀      | 左结合 | 管道符：流式数据清洗计算                       |
| **4**         | `<`, `>`, `<=`, `>=` | 二元中缀      | 左结合 | 大小关系比较                                   |
| **3**         | `==`, `!=`           | 二元中缀      | 左结合 | 相等/不等判断（严格类型安全）                  |
| **2**         | `&&`                 | 二元中缀      | 左结合 | 短路逻辑与                                     |
| **1**         | `\|\|`               | 二元中缀      | 左结合 | 短路逻辑或                                     |
| **0**         | `??`                 | 二元中缀      | 左结合 | 空值合并：左侧非 `null` 返回左侧，否则返回右侧 |
| **-1**        | `? :`                | 三元中缀      | 右结合 | 条件分支控制流表达式                           |
| **-2 (最低)** | `->`                 | 二元中缀      | 右结合 | Lambda 箭头：具备极强向右吞噬能力              |

---

### 约束

- **无副作用**: DEL 表达式是无副作用的纯计算语句，不包含任何副作用操作。
- **无状态**: DEL 表达式不维护任何状态，不包含任何状态操作。
- **无递归**: DEL 表达式不包含递归调用，不包含任何递归操作。
- **类型匹配**: DEL 表达式关系运算符要求两侧操作数类型匹配，否则抛出异常。
  - `@/num == "1"` ➜ **运行时抛出异常**（类型不匹配）。
  - `true || "true"` ➜ **运行时抛出异常**（非布尔值参与逻辑运算）。

## 符号表

DEL 的高阶操作均由内置函数实现，每个函数均接受一个或多个参数，并返回一个值。

> 函数名区分大小写，禁止

### 内置函数

#### `remove_char` - 移除字符串中的指定字符

- 原型: `remove_char(str, char_str)`
  - `char_str`: `string`
    - 参数为单个字符，不支持字符串。
- 返回: `string`
  - 移除指定字符后的字符串。

```del
@/foo/bar |> remove_char('-')
```

#### `to_lower` - 将字符串转为小写

- 原型: `to_lower(str)`
  - `str`: `string`
- 返回: `string`
  - 转为小写后的字符串。

```del
// 正常写法
@/foo/bar |> to_lower()

// 当单参数函数配合管道运算符时，可省略空括号
@/foo/bar |> to_lower
```

#### `to_upper` - 将字符串转为大写

- 原型: `to_upper(str)`
  - `str`: `string`
- 返回: `string`
  - 转为大写后的字符串。

```del
@/foo/bar |> to_upper
```

#### `trim` - 去除字符串首尾的空白字符

- 原型: `trim(str)`
  - `str`: `string`
- 返回: `string`
  - 去除首尾空白字符后的字符串。

```del
@/foo/bar |> trim
```

#### `remove_suffix` - 从字符串中移除指定后缀

- 原型: `remove_suffix(str, suffix_str)`
  - `str`: `string`
  - `suffix_str`: `string`
- 返回: `string`
  - 移除指定后缀后的字符串。

```del
@/foo/bar |> remove_suffix('abcd')
```

#### `is_null` - 判断参数是否为 null

- 原型: `is_null(any)`
  - `any`: `any`
- 返回: `bool`
  - 参数是否为 null。

> 此函数通常用于控制流中，例如：

```del
is_null(@/foo/bar) ? ... : ...
```

#### `to_str` - 将任意类型转为字符串

- 原型: `to_str(any)`
  - `any`: `any`
- 返回: `string`
  - 将任意类型转为字符串。

> 对于字符串类型，此函数将直接返回原值（避免二次转义嵌套）

```del
to_str(@/foo/bar)

//  或者使用管道
@/foo/bar |> to_str
```

#### `entry` - 创建一个 JSON 键值对对象

- 原型: `entry(key, value)`
  - `key`: `string`
    - 参数必须为字符串类型。
  - `value`: `any`
- 返回: `object`
  - 创建的键值对对象。

```del
entry('foo', 'bar')
// 返回: {"foo":"bar"}
```

#### `map` - 遍历数组中的元素并返回一个新数组

- 原型: `map(array, lambda)`
  - `array`: `array`
  - `lambda`: `(element, index) -> ...`
    - `element`: `T`
      - 指向 `array` 中的每一个元素(迭代), `element` 类型取决于输入类型。
    - `index` : `number`
      - 指向 `array` 中的每一个元素(迭代)的索引, 此参数可选。
- 返回: `array`
  - 遍历数组中的元素并返回一个新数组。

```del
// 可省略 index 下标参数
@/foo/bar |> map((element) -> element |> to_upper)

@/foo/bar |> map((element, index) -> element |> to_upper)
```

#### `map_object` - 遍历对象中的键值对并返回一个新对象

- 原型: `map_object(obj, lambda)`
  - `obj`: `object`
  - `lambda`: `(key, value, index) -> ...`
    - `key`: `string`
      - 指向 `obj` 中的每一个键(迭代)。
    - `value`: `T`
      - 指向 `obj` 中的每一个值(迭代)。
    - `index`: `number`
      - 指向 `obj` 中的每一个键值对(迭代)的索引, 此参数可选。
- 返回: `object`
  - 遍历对象中的键值对并返回一个新对象。

> ⚠️ 注意：根据 JSON RFC 8259 规范，对象是无序的。`index` 下标将依据底层容器的**键（Key）字母表字典序**递增，不建议在对顺序强敏感的业务中使用。

```del
// 可省略 index 下标参数
@/foo/bar |> map_object((key, value) -> entry(key |> remove_suffix('_'), value))

// 输入:
// { "x_": 1, "y_": 2, "z_": 3 }
// 返回:
// { "x": 1, "y": 2, "z": 3 }
```

#### `object` - 创建一个空的 JSON 对象

- 原型: `object()`
- 返回: `object`

```del
object()
// 返回: {}
```

#### `array` - 创建一个空的 JSON 数组

- 原型: `array()`
- 返回: `array`

```del
array()
// 返回: []
```

#### `put` - 向 JSON 容器中添加键值对

- 原型: `put(container, key_index, value)`
  - `container`: `object` 或 `array`
  - `key_index`: `string` 或 `number`
  - `value`: `any`
- 返回: `object` 或 `array`
  - 添加键值对后的 JSON 容器。

```del
// 对新建的对象添加键值对
object() |> put('foo', 'bar') // {"foo":"bar"}

// 对新建的数组添加元素
array() |> put(0, 'foo') // ["foo"]
```

#### `reduce` - 遍历数组并返回一个新容器

- 原型: `reduce(array, init, lambda)`
  - `array`: `array`
    - 要遍历的数组
  - `init`: `any`
    - 新容器的初始值
  - `lambda`: `(accumulator, element, index) -> ...`
    - `accumulator`: `any`
      - 新容器的当前值
    - `element`: `T`
      - 指向 `array` 中的每一个元素(迭代), `element` 类型取决于输入类型。
    - `index` : `number`
      - 指向 `array` 中的每一个元素(迭代)的索引, 此参数可选。
- 返回: `any`
  - 遍历数组并返回一个新容器。

```del
// 将数组转为对象
array() |> put(0, 'foo') |> reduce(object(), (acc, el) -> put(acc, el, true)) // {"foo":true}
```

---

## 语法糖

### 1. 管道符

在管道符中，左侧表达式的求值结果，会自动作为右侧函数的第一个隐式参数注入。

如果右侧函数具有多个参数，则左侧表达式的求值结果会隐式作为第一个参数，后续参数需要显式传递。

如果右侧的函数仅有一个参数，可以省略括号。

```del
@/foo/bar |> remove_suffix('-') // 等价于 remove_suffix(@/foo/bar, '-')

@/foo/bar |> to_upper // 等价于 to_upper(@/foo/bar)
```
