#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define strmaxsize 			3000			 // 预估原始输入文本容量
#define charmaxsize 		128			     // 预估字符种类个数
#define strmaxsize_temp 	strmaxsize * 6	 // 伪二进制编码流总体大小
#define paragraph           800			     // 一段文字容量
#define codesize            20				 // 编码最大长度


/* 字符编码信息*/
typedef struct Huff_char {
    char character;
    int count;
    char code[20];
} Huff_char;

/** 字符队列 **/
typedef struct Huffchar {
    char character;
    int count;
    struct Hufftree* self;
} Huffchar;

/** 哈夫曼树节点 动态（静态）数组描述Huffman树 **/
typedef struct Hufftree {
    char character;	          // 节点字符信息
    int power;		          // 权值
    struct Hufftree* self;
    struct Hufftree* rchild;
    struct Hufftree* lchild;  // 自身地址，左右孩子

} Hufftree; 

void buildTree(Huffchar chartype[]);
void dfs(Hufftree * Head, int lev);
void getFrequency();
int getMinArr(Huffchar chartype[], Huffchar minarr[]);
void gettext();
void readfile(int op);
void converToBit();
void outfile(int op);
void decode();
void puttree();
void putcode();
void bulid_detree();

char str_text[strmaxsize];					//原始输入文本
struct Huffchar chartype[charmaxsize];		//字符队列临时数据
struct Huff_char char_type[charmaxsize];	//统计字符种类个数
Hufftree * Root,*de_root;					//哈夫曼树根节点
char code_temp[7];							//临时变量存储编码
char strbit_temp[strmaxsize_temp];			//伪二进制编码流
char de_strbit_temp[strmaxsize_temp];		//伪二进制流数组
char strcode[strmaxsize];					//编码后压缩字符串
char de_strcode[strmaxsize];				//解码后恢复字符串
char de_str_text[strmaxsize];				//编码后原始文本
char mabiao[charmaxsize][codesize];			//存储码表的数组

int main(int argc,char *argv[])
{
    int arg_cnt = 4;
    if (arg_cnt < argc) {
        printf("input error. argv: [%d]\n eg: escape file.txt reserved_chars\n",
               argc);
        return -1;
    }

    int op = 0;
    char method[125] = {0};
    char path[125] = {0};
    char reserved_chars[512] = {0};

    strncpy(method, argv[1], sizeof(method) - 1);
    strncpy(path, argv[2], sizeof(path) - 1);
    strncpy(reserved_chars, argv[3], sizeof(reserved_chars) - 1);

    if (0 == strcmp(method, "escape")) {
        op = 0;
    } else if (0 == strcmp(method, "unescape")) {
        op = 1;
    } else {
        printf("method input error.\n eg: escape/unescape\n");
        return -1;
    }

    readfile(op);
    getFrequency();
    buildTree(chartype);
    dfs(Root, 0);
    puttree();
    putcode();
    converToBit();
    bulid_detree();
    decode();

    return 0;
}

/**
输出字符对应的词频与编码
**/
void puttree()
{
    for (size_t i = 0; i < charmaxsize; i++) {
        if (char_type[i].character > 0) {
            // printf("%c---%d---", char_type[i].character, char_type[i].count);
            puts(char_type[i].code);
        }
    }
    printf("\n");
}

/**  
输出码表
**/
void putcode()
{
    FILE *fp;
    fp = fopen("./code.txt","w");
    if (fp) {
        for (size_t i = 0; i < charmaxsize; i++) {
            if (char_type[i].character > 0) {
                fprintf(fp,"%c", char_type[i].character);
                fputs(char_type[i].code,fp);
                fprintf(fp,"\n");
            }
        }
        fclose(fp);
    } else {
        printf("码表输出失败");
    }
}

/**
 读取码表建树
**/
void bulid_detree()
{
    FILE *fp = NULL;
    de_root = (Hufftree *)malloc(sizeof(Hufftree));
    Hufftree* p = de_root;
    Hufftree* t;
    int tag = 0;
    de_root->lchild = de_root->rchild = NULL;
    int i = 0;
    int j = 1;
    int k = 0;

    fp = fopen("./test.txt", "r");
    if (fp) {
        for ( i = 0; i < charmaxsize; i++) {

            if ((fgets(mabiao[i], codesize, fp)) == NULL) {
                break;
            } else if (tag) {
                for ( k = 0; k < codesize-1; k++) {
                    mabiao[i][codesize - 1 - k] = mabiao[i][codesize - 2 - k];
                }
                mabiao[i][0] = mabiao[i-1][0];
                tag = 0;
            }

            if (mabiao[i][1] == '\0') {
                tag = 1;
            }
        }

        fclose(fp);
        i = 0;

        /** 根据码表建树 **/
        while (mabiao[i][0] != '\0') {
            p = de_root;

            while (mabiao[i][j] != '\0') {
                if (mabiao[i][j] == '1') {
                    if (p->rchild == NULL) {
                        t = (Hufftree*)malloc(sizeof(Hufftree));
                        t->lchild = t->rchild = NULL;
                        p->rchild = t;
                        p = p->rchild;

                    } else {
                        p = p->rchild;
                    }
                }

                if (mabiao[i][j] == '0') {
                    if (p->lchild == NULL) {
                        t = (Hufftree*)malloc(sizeof(Hufftree));
                        t->lchild = t->rchild = NULL;
                        p->lchild = t;
                        p = p->lchild;
                    } else {
                        p = p->lchild;
                    }
                }
                j++;
            }

            p->character = mabiao[i][0];
            j = 1;
            i++;
        }

    } else {
        printf("码表文件读取失败，直接获取Huffman树\n");
        de_root = Root;

        return;
    }

}

/**
* 分析字符频率,创建字符队列
**/
void getFrequency()
{
    int i = 0;
    int j = 0;
    int k = 0;
    int temp = 0;
    int tag = 0;//标记是否存在

    while (str_text[i] != '\0') {
        for (temp = 0; temp < charmaxsize; temp++) {
            tag = 0;
            for (k = 0; k < j; k++) {
                if (str_text[i] == chartype[k].character) {
                    chartype[k].count++;
                    tag = 1;
                    break;
                }
            }
            if (tag)
                break;
            if (str_text[i] == temp) {
                chartype[j].character = temp;
                chartype[j].count++;
                j++;
                break;
            }
        }
        i++;
    }
}

/**
* 深度优先遍历叶子节点,保存字符编码
**/
void dfs(Hufftree * node, int lev)
{
    int i = 0;
    static int node_i = 0;

    if (node->lchild == node->rchild) {
        char_type[node_i].character = node->character;
        char_type[node_i].count = node->power;
        for (i = 0; i < lev; i++) {
            char_type[node_i].code[i] = code_temp[i];
        }
        char_type[node_i].code[i] = '\0';
        node_i++;
    } else {
        if (node->lchild) {
            code_temp[lev] = '0';
            dfs(node->lchild, lev + 1);
        }
        if (node->rchild) {
            code_temp[lev] = '1';
            dfs(node->rchild, lev + 1);
        }
    }
}

/**
* 根据字符词频建树
**/
void buildTree(Huffchar chartype[])
{
    int max = 2;
    Huffchar minarr[2];//两个最小字符结点
    Hufftree *rp = NULL;
    Hufftree *lp = NULL;
    Hufftree *p  = NULL;

    if (chartype[1].count == 0) {
        if (NULL != (lp = (Hufftree *)malloc(sizeof(Hufftree)))) {
            lp->character = chartype[0].character;
            lp->power = chartype[0].count;
            lp->self = NULL;
            lp->lchild = NULL;
            lp->rchild = NULL;
        } else {
            printf("内存分配错误！");
            exit(0);
        }

        if ( NULL != (p = (Hufftree *)malloc(sizeof(Hufftree)))) {
            p->character = 0;
            p->power = chartype[0].count;
            p->self = NULL;
            p->lchild = lp;
            p->rchild = NULL;
            chartype[0].self = p;
        } else {
            printf("内存分配错误！");
            exit(0);
        }
    }

    while (chartype[1].count > 0) { //当数组剩余2个及以上输出最小节点
        max = getMinArr(chartype, minarr);

        //右子树填充最小值
        if (NULL != (rp = (Hufftree *)malloc(sizeof(Hufftree)))) {
            if (minarr[0].self != NULL)//不为叶子节点
                rp = minarr[0].self;
            else {
                rp->character = minarr[0].character;
                rp->power = minarr[0].count;
                rp->rchild = NULL;
                rp->lchild = NULL;
            }
        } else {
            printf("内存分配错误！");
            exit(0);
        }

        //左子树填充次小值
        if (NULL != (lp = (Hufftree *)malloc(sizeof(Hufftree)))) {
            if (minarr[1].self != NULL) { //不为叶子节点
                lp = minarr[1].self;
            } else {
                lp->character = minarr[1].character;
                lp->power = minarr[1].count;
                lp->rchild = NULL;
                lp->lchild = NULL;
            }
        } else {
            printf("内存分配错误！");
            exit(0);
        }
        //更新父节点权值
        if (NULL != (p = (Hufftree *)malloc(sizeof(Hufftree)))) {
            p->character = 0;
            p->power = minarr[0].count + minarr[1].count;
            p->self = p;
            p->lchild = lp;
            p->rchild = rp;
        } else {
            printf("内存分配错误！");
            exit(0);
        }

        //更新字符数组,将两个最小节点作为整体放入数组
        chartype[max].character = 0;//非叶子字符数据记录NULL；
        chartype[max].count = minarr[0].count + minarr[1].count;
        chartype[max].self = p;
    }

    Root = chartype[0].self;
}

/**
 依次返回两个最小节点数据，删除原数组元素,并返回当前元素个数
**/
int getMinArr(Huffchar chartype[], Huffchar minarr[])
{
    int i = 0;
    int min;//记录最小节点位置
    int max = 2;//记录数组元素个数
    Huffchar temp;

    //直接排序选出最小结点
    minarr[0] = chartype[0];
    min = 0;
    while (chartype[i].count > 0) {
        if (minarr[0].count > chartype[i].count) {
            minarr[0] = chartype[i];
            min = i;
        }
        i++;
    }

    //删除min结点
    while (chartype[min + 1].count > 0) {
        chartype[min] = chartype[min + 1];
        min++;
    }
    chartype[min].count = 0;

    //直接排序选出次小结点
    minarr[1] = chartype[0];
    min = i = 0;


    while (chartype[i].count > 0) {
        if (minarr[1].count > chartype[i].count) {
            minarr[1] = chartype[i];
            min = i;
        }
        i++;
    }

    //删除min结点
    while (chartype[min + 1].count > 0) {
        chartype[min] = chartype[min + 1];
        min++;
    }
    chartype[min].count = 0;
    return min;
}

/**
* 获取字符流
**/
void gettext()
{
    printf("请输入编码文本：");
    gets(str_text);

}

/**
 输出压缩后的字符
**/
void outfile(int op)
{
    int i = 0;
    FILE* fp;
    if (!op) {
        fp = fopen("./outputfile.txt", "w");
        fputs(strcode, fp);
    } else {
        fp = fopen("./decodefile.txt", "w");
        fputs(de_strcode, fp);
    }
    fclose(fp);
}

/**
* 获取源文档字符
**/
void readfile(int op)
{
    FILE*    fp = NULL;
    int text_i  = 0;
    char tempc  = 0;

    if (!op) {
        fp = fopen("./input.txt", "r");
    } else {
        fp = fopen("./output.txt", "r");
    }

    if (fp != NULL) {
        while (fscanf(fp, "%c", &tempc) != EOF) {
            if (!op) {
                str_text[text_i] = tempc;
                text_i++;
            } else {
                de_str_text[text_i] = tempc;
                text_i++;
            }
        }
        de_str_text[text_i] = '\0';
        fclose(fp);

    } else {
        if (!op) {
            // printf("待编码文件读取失败！\n");
            gettext();
        } else {
            // printf("已编码文件读取失败！，直接获取已编码字符串\n");
            strcpy(de_str_text, strcode);
        }
    }
}

/**
* 字符编码后转化为二进制流
**/
void converToBit()
{
    int text_i = 0;
    int char_i = 0;
    int i = 0;
    int j = 0;
    int num = 0;
    bool tag = true;
    char tempc;
    strbit_temp[0] = '\0';

    /** 转换成伪二进制流。**/
    for (text_i = 0; text_i < strmaxsize; text_i++) {
        for (char_i = 0; char_i < charmaxsize; char_i++) {
            if (char_type[char_i].character == str_text[text_i]) {
                strcat(strbit_temp, char_type[char_i].code);
            }
        }
    }

    //转换成二进制流，ASCII码形式输出
    text_i = 0;
    while (tag) {
        for (i = 0; i < 6; i++, text_i++) {
            if (strbit_temp[text_i] != '\0')
            {
                num += ((int)strbit_temp[text_i] - 48)*(1 << (5 - i));
            }
            else
            {
                tag = false;
                break;
            }
        }
        tempc = (char)num + 31;
        strcode[j++] = tempc;
        num = 0;
    }
    strcode[j] = '\0';
    printf("压缩后编码为：");
    puts(strcode);
    outfile(0);
}

/** 压缩文件解码 **/
void decode()
{
    int bit_i = 0;
    int str_i = 0;
    int i    = 0;
    char temp[7] = {0};
    int asc      = 0;
    Hufftree *seek = NULL;

    readfile(1);

    de_strbit_temp[0] = '\0';

    // 转化为伪二进制数组
    for (str_i = 0; str_i < strmaxsize; str_i++) {
        if (de_str_text[str_i] != '\0') {
            asc = (int)de_str_text[str_i] - 31;
            for (i = 0; i < 6; i++) {
                if (asc % 2 == 0) {
                    temp[5 - i] = '0';
                } else {
                    temp[5 - i] = '1';
                }
                asc = asc / 2;
            }
            temp[6] = '\0';
            strcat(de_strbit_temp, temp);
        } else {
            break;
        }
    }

    //按照huffman树转换成原文本
    seek = de_root;
    str_i = 0;
    while (de_strbit_temp[bit_i] != '\0') {
        if (seek == NULL)
            return;
        if (seek->lchild == seek->rchild) {
            de_strcode[str_i] = seek->character;
            str_i++;
            seek = de_root;
        } else if (de_strbit_temp[bit_i] == '1') {
            seek = seek->rchild;
            bit_i++;
        } else {
            seek = seek->lchild;
            bit_i++;
        }
    }

    de_strcode[str_i] = '\0';

    // 输出解码文件
    printf("解压后文本为：");
    puts(de_strcode);
    outfile(1);
}

