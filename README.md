# mrubyc-gem-ili934x
mruby/c sources for ili934x (LCD used M5Stack basic)

## History (en)
Add font display fucntion for 2byte-characters
Only Japanese 常用漢字 and ひらがな/カタカナ, ascii characters
Used PixelMplus12-Regular.ttf (M+ FONT LICENSE)
http://mplus-fonts.osdn.jp/mplus-bitmap-fonts/#license
Generated font.h by https://github.com/rikutons/TTFtoBitMap
Make drawing-figure functions faster by changing ruby-implements to clang-implements

## History (ja)
2バイト文字の描画機能を追加
ひらがな, カタカナ, 常用漢字, ASCII文字のみ対応
PixelMplus12-Regular.ttf (M+ FONT LICENSE) を使用
http://mplus-fonts.osdn.jp/mplus-bitmap-fonts/#license
https://github.com/rikutons/TTFtoBitMap を用いてfont.hを生成
Rubyでの実装をc言語での実装に変更することで描画機能を高速化
