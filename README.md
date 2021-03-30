## 小作业：实现一个编解码工具，对纯文本文件进行 escape/unescape。

escape 命令： tool escape   file.txt reserved_chars
unescape 命令： tool unescape  file.txt reserved_chars

要求：
1. escape 之后不含有 reserved_chars 中的任何字符，且不向文件添加原来没有的字符
2. 对于不同的文件内容以及 reserved_chars，尽量覆盖所有 corner case (包括无法 escape 的情况)

加分项：
1. 编码后的文件越短越好
2. 代码复杂度越低越好

## 基于霍夫曼编码对文本进行压缩/解压缩
