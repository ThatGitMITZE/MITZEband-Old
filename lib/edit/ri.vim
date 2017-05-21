if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif
syn case match

syn match riComment /^#.*$/
syn match riInclude /%:.*$/
syn match riNumber /\d\+/ contained
syn match riDice /\d\+d\d\+/ contained 
syn match riPercent /\d\+%/ contained 

syn region riNLine start=/^N:/ end=/$/ contains=riNumber

syn keyword riBMethod HIT TOUCH PUNCH KICK CLAW BITE STING SLASH BUTT CRUSH ENGULF contained
syn keyword riBMethod CHARGE CRAWL DROOL SPIT EXPLODE GAZE WAIL SPORE BEG INSULT MOAN SHOW contained
syn match riBEffect /UN_BONUS/ contained
syn match riBEffect /TERRIFY/ contained
syn match riBEffect /LOSE_STR/ contained
syn match riBEffect /LOSE_INT/ contained
syn match riBEffect /LOSE_WIS/ contained
syn match riBEffect /LOSE_DEX/ contained
syn match riBEffect /LOSE_CON/ contained
syn match riBEffect /LOSE_CHR/ contained
syn match riBEffect /LOSE_ALL/ contained
syn region riBExp matchgroup=riOp start=/SUPERHURT(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/HURT(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/POISON(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/UN_BONUS(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/UN_POWER(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/EAT_GOLD(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/EAT_ITEM(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/EAT_FOOD(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/EAT_LITE(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/ACID(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/ELEC(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/FIRE(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/COLD(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/BLIND(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/CONFUSE(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/TERRIFY(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/PARALYZE(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/LOSE_STR(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/LOSE_INT(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/LOSE_WIS(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/LOSE_DEX(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/LOSE_CON(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/LOSE_CHR(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/LOSE_ALL(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/SHATTER(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/EXP_10(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/EXP_20(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/EXP_40(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/EXP_80(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/DISEASE(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/TIME(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/VAMP(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/DR_MANA(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/CUT(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/STUN(/ end=/)/ contains=riDice,riPercent contained
syn region riBLine matchgroup=riLinePrefix start=/^B:/ end=/$/ contains=riBMethod,riBExp,riBEffect

syn region riWLine matchgroup=riLinePrefix start=/^W:/ end=/$/ contains=riNumber

hi def link riOp Operator
hi def link riLinePrefix Type

hi def link riComment Comment
hi def link riBMethod Identifier
hi def link riBEffect Type
hi def link riFilename Identifier
hi def link riInclude PreProc
hi def link riNumber Number
hi def link riDice Number
hi def link riPercent Number
hi def link riNLine Special
let b:current_syntax = "ri"

