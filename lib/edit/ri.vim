if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif
syn case match

syn keyword riError DEPRECATED
syn match riComment /^#.*$/
syn match riInclude /%:.*$/
syn match riNumber /\d\+/ contained
syn match riDice /\d\+d\d\+/ contained 
syn match riPercent /\d\+%/ contained 

syn region riNLine start=/^N:/ end=/$/ contains=riNumber

syn keyword riBMethod HIT TOUCH PUNCH KICK CLAW BITE STING SLASH BUTT CRUSH ENGULF contained
syn keyword riBMethod CHARGE CRAWL DROOL SPIT EXPLODE GAZE WAIL SPORE BEG INSULT MOAN SHOW contained
syn match riBEffect /BLIND/ contained
syn match riBEffect /CONFUSE/ contained
syn match riBEffect /DISEASE/ contained
syn match riBEffect /DISENCHANT/ contained
syn match riBEffect /DRAIN_CHARGES/ contained
syn match riBEffect /EXP_10/ contained
syn match riBEffect /EXP_20/ contained
syn match riBEffect /EXP_40/ contained
syn match riBEffect /EXP_80/ contained
syn match riBEffect /LOSE_STR/ contained
syn match riBEffect /LOSE_INT/ contained
syn match riBEffect /LOSE_WIS/ contained
syn match riBEffect /LOSE_DEX/ contained
syn match riBEffect /LOSE_CON/ contained
syn match riBEffect /LOSE_CHR/ contained
syn match riBEffect /LOSE_ALL/ contained
syn match riBEffect /PARALYZE/ contained
syn match riBEffect /TERRIFY/ contained
syn match riBEffect /TIME/ contained
syn region riBExp matchgroup=riOp start=/SUPERHURT(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/HURT(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/POISON(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/DISENCHANT(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/DRAIN_CHARGES(/ end=/)/ contains=riDice,riPercent contained
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
syn region riBExp matchgroup=riOp start=/EXP_VAMP(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/DR_MANA(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/CUT(/ end=/)/ contains=riDice,riPercent contained
syn region riBExp matchgroup=riOp start=/STUN(/ end=/)/ contains=riDice,riPercent contained
syn region riBLine matchgroup=riLinePrefix start=/^B:/ end=/$/ contains=riBMethod,riBExp,riBEffect

syn region riAExp matchgroup=riOp start=/ACID(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/AMNESIA(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/BRAIN_SMASH(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/CAUSE_1(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/CAUSE_2(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/CAUSE_3(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/DARK(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/DISENCHANT(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/ELDRITCH(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/HELL_FIRE(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/HOLY_FIRE(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/LITE(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/MIND_BLAST(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/NETHER(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/SOUND(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/SHARDS(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/STUN(/ end=/)/ contains=riDice,riPercent contained
syn region riAExp matchgroup=riOp start=/TIME(/ end=/)/ contains=riDice,riPercent contained
syn region riALine matchgroup=riLinePrefix start=/^A:/ end=/$/ contains=riAExp,riAEffect

syn region riWLine matchgroup=riLinePrefix start=/^W:/ end=/$/ contains=riNumber

hi def link riOp Operator
hi def link riLinePrefix Type
hi def link riError Error
hi def link riComment Comment

hi def link riBMethod Identifier
hi def link riBEffect Type
hi def link riAEffect Type
hi def link riFilename Identifier

hi def link riInclude PreProc
hi def link riNumber Number
hi def link riDice Number
hi def link riPercent Number
hi def link riNLine Special
let b:current_syntax = "ri"

