#!/bin/sh
###################################################
# 调用本地的valgrind软件，对指定的app进行内存监测
# 默认执行utKichenMachine_linuxd应用程序
# 在执行app路径上生成同名日志脚本
##################################################
#探测本地是否有vagrind
match=$(valgrind --version|grep -c "valgrind");
if [ $match -ge 1 ];then
    echo "valgrind exist";
    echo -n "app full path name or use default<d>:";
    read userpath;
else 
    echo "valgrind not found";
    exit 0;
fi

if [[ $userpath == "d" ]];then
    #搜索默认路径
    cd ..;
    hitoutdir=$(ls | grep -c -E "^output$");
    echo $hitoutdir
    if [ $hitoutdir -eq 1 ];then
        userpath=$(pwd)/output/utKichenMachine_linuxd;
    fi
    cd - 1>/dev/null;
    echo $userpath
fi

if [ -x $userpath ];then
    #可执行文件名称
    cd ${userpath%/*}
    elffn=${userpath##*/};
    valgrind --tool=memcheck --leak-check=full --log-file=$elffn$$.lg ./$elffn;
    
    #生成delete free delete[]不配对文件
    date >$elffn$$.mismatch.lg
    echo "内存释放错误使用的位置："
    echo "Mismatched:内存释放delete free使用未配对"
    echo "Invalid free:同一段内存释放两次"
    cat $elffn$$.lg|awk '/==.*==.*Mismatched/,/==.*== *$/'>>$elffn$$.mismatch.lg
    cat $elffn$$.lg|awk '/==.*==.*Invalid free/,/==.*== *$/'>>$elffn$$.mismatch.lg
    #生成内存泄露部分的文件
    date >$elffn$$.leak.lg
    echo "内存泄露的位置："
    cat $elffn$$.lg|awk '/==.*==.*definitely lost/,/==.*== *$/'>>$elffn$$.leak.lg
    #生成内存越界访问部分的文件
    date >$elffn$$.badwrite.lg
    echo "内存越界操作的位置："
    cat $elffn$$.lg|awk '/==.*==.*Invalid write/,/==.*== *$/'>>$elffn$$.badwrite.lg
    #生成条件语句未初始化部分
    date >$elffn$$.uinit.lg
    echo "条件语句未初始化位置："
    cat $elffn$$.lg|awk '/==.*==.*Conditional jump/,/==.*== *$/'>>$elffn$$.uinit.lg
    
    cd - 1>/dev/null
else
    #不可执行文件，用户选择错误
    echo "bad format file be run";
fi

exit 0;
