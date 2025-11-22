#!/bin/bash

# 使用pkexec获取权限并执行安装命令
pkexec bash <<EOF
# 更新软件包列表
apt update

# 安装fcitx5和fcitx5-rime
apt install fcitx5 fcitx5-rime -y
EOF

# 提示用户安装完成
echo "中州韵输入法引擎安装成功！"