#include "../include/client.hpp"
#include <iostream>
#include <thread>

/*
  关于查看下载结果：
  直接双击 deepseek.html 在浏览器打开会显示 404 / 空白，原因：
  1. 页面内嵌脚本检测到 URL 路径不是站点根路径 "/"，会自动跳转到 /404/；
  2. 页面正文默认 style="opacity:0"，靠 JS 加载后恢复显示（淡入动画）。

  想看内容：请使用以下指令

  python3 - <<'EOF'
  import re
  d = open('deepseek.html', encoding='utf-8').read()
  d = re.sub(r'<script>.*?</script>', '', d, flags=re.S)   # 删掉跳转/显示脚本
  d = d.replace('style="opacity:0"', 'style="opacity:1"')  # 最外层隐藏容器(无分号)
  d = d.replace('style="opacity:0;', 'style="opacity:1;')  # 其他隐藏区块(带分号)
  open('deepseek_view.html', 'w', encoding='utf-8').write(d)
  EOF
  xdg-open deepseek_view.html

  CSS / 图片等外链资源没有下载，页面是无样式的纯文字，属正常现象。
*/
int main()
{

  download::Worker wk("www.deepseek.com", "deepseek.html", 2);

  try{
    wk.run();
  }
  catch(std::runtime_error& e){
    std::cout << e.what() << std::endl;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "END" << std::endl;


}
