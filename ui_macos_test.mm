#include "ui.hpp"
#include <Cocoa/Cocoa.h>
#include "common.hpp"
#include "list.hpp"
#include <harfbuzz/hb.h>
#include <harfbuzz/hb-coretext.h>
#include "defer.hpp"
#include <fontconfig/fontconfig.h>

char* parse_hex(ccstr s) {
    int len = strlen(s);
    auto ret = alloc_array(char, len/2+1);
    int i = 0, j = 0;

    for (; i<len; i += 2) {
        while (isspace(s[i]) && i<len) i++;
        if (i>=len) break;

        char tmp[3] = { s[i+0], s[i+1], 0 };
        ret[j++] = (char)strtol(tmp, NULL, 16);
    }

    ret[j] = 0;
    return ret;
}

ccstr big_string =
    u8"ㄅㄆㄇㄈㄉㄊㄋㄌㄍㄎㄏㄐㄑㄒㄓㄔㄕㄖㄗㄘㄙㄚㄛㄜㄝㄞㄟㄠㄡㄢㄣㄤ"
    u8"ㄥㄦㄧㄨㄩㄪㄫㄬㄭㄮㄯㆠㆡㆢㆣㆤㆥㆦㆧㆨㆩㆪㆫㆬㆭㆮㆯㆰㆱㆲㆳㆴ"
    u8"ㆵㆶㆷㆸㆹㆺㆻㆼㆽㆾㆿ一丁丂七丄丅丆万丈三上下丌不与丏丐丑丒专且"
    u8"丕世丗丘丙业丛东丝丞丟丠両丢丣两严並丧丨丩个丫丬中丮丯丰丱串丳临"
    u8"丵丶丷丸丹为主丼丽举丿乀乁乂乃乄久乆乇么义乊之乌乍乎乏乐乑乒乓乔"
    u8"乕乖乗乘乙乚乛乜九乞也习乡乢乣乤乥书乧乨乩乪乫乬乭乮乯买乱乲乳乴"
    u8"乵乶乷乸乹乺乻乼乽乾乿亀亁亂亃亄亅了亇予争亊事二亍于亏亐云互亓五"
    u8"井亖亗亘亙亚些亜亝亞亟亠亡亢亣交亥亦产亨亩亪享京亭亮亯亰亱亲亳亴"
    u8"亵亶亷亸亹人亻亼亽亾亿什仁仂仃仄仅仆仇仈仉今介仌仍从仏仐仑仒仓仔"
    u8"仕他仗付仙仚仛仜仝仞仟仠仡仢代令以仦仧仨仩仪仫们仭仮仯仰仱仲仳仴"
    u8"仵件价仸仹仺任仼份仾仿伀企伂伃伄伅伆伇伈伉伊伋伌伍伎伏伐休伒伓伔"
    u8"伕伖众优伙会伛伜伝伞伟传伡伢伣伤伥伦伧伨伩伪伫伬伭伮伯估伱伲伳伴"
    u8"伵伶伷伸伹伺伻似伽伾伿佀佁佂佃佄佅但佇佈佉佊佋佌位低住佐佑佒体佔"
    u8"何佖佗佘余佚佛作佝佞佟你佡佢佣佤佥佦佧佨佩佪佫佬佭佮佯佰佱佲佳佴"
    u8"併佶佷佸佹佺佻佼佽佾使侀侁侂侃侄侅來侇侈侉侊例侌侍侎侏侐侑侒侓侔"
    u8"侕侖侗侘侙侚供侜依侞侟侠価侢侣侤侥侦侧侨侩侪侫侬侭侮侯侰侱侲侳侴"
    u8"侵侶侷侸侹侺侻侼侽侾便俀俁係促俄俅俆俇俈俉俊俋俌俍俎俏俐俑俒俓俔"
    u8"俕俖俗俘俙俚俛俜保俞俟俠信俢俣俤俥俦俧俨俩俪俫俬俭修俯俰俱俲俳俴"
    u8"俵俶俷俸俹俺俻俼俽俾俿倀倁倂倃倄倅倆倇倈倉倊個倌倍倎倏倐們倒倓倔"
    u8"倕倖倗倘候倚倛倜倝倞借倠倡倢倣値倥倦倧倨倩倪倫倬倭倮倯倰倱倲倳倴"
    u8"倵倶倷倸倹债倻值倽倾倿偀偁偂偃偄偅偆假偈偉偊偋偌偍偎偏偐偑偒偓偔"
    u8"偕偖偗偘偙做偛停偝偞偟偠偡偢偣偤健偦偧偨偩偪偫偬偭偮偯偰偱偲偳側"
    u8"偵偶偷偸偹偺偻偼偽偾偿傀傁傂傃傄傅傆傇傈傉傊傋傌傍傎傏傐傑傒傓傔"
    u8"傕傖傗傘備傚傛傜傝傞傟傠傡傢傣傤傥傦傧储傩傪傫催傭傮傯傰傱傲傳傴"
    u8"債傶傷傸傹傺傻傼傽傾傿僀僁僂僃僄僅僆僇僈僉僊僋僌働僎像僐僑僒僓僔"
    u8"僕僖僗僘僙僚僛僜僝僞僟僠僡僢僣僤僥僦僧僨僩僪僫僬僭僮僯僰僱僲僳僴"
    u8"僵僶僷僸價僺僻僼僽僾僿儀儁儂儃億儅儆儇儈儉儊儋儌儍儎儏儐儑儒儓儔"
    u8"儕儖儗儘儙儚儛儜儝儞償儠儡儢儣儤儥儦儧儨儩優儫儬儭儮儯儰儱儲儳儴"
    u8"儵儶儷儸儹儺儻儼儽儾儿兀允兂元兄充兆兇先光兊克兌免兎兏児兑兒兓兔"
    u8"兕兖兗兘兙党兛兜兝兞兟兠兡兢兣兤入兦內全兩兪八公六兮兯兰共兲关兴"
    u8"兵其具典兹兺养兼兽兾兿冀冁冂冃冄内円冇冈冉冊冋册再冎冏冐冑冒冓冔"
    u8"冕冖冗冘写冚军农冝冞冟冠冡冢冣冤冥冦冧冨冩冪冫冬冭冮冯冰冱冲决冴"
    u8"况冶冷冸冹冺冻冼冽冾冿净凁凂凃凄凅准凇凈凉凊凋凌凍凎减凐凑凒凓凔"
    u8"凕凖凗凘凙凚凛凜凝凞凟几凡凢凣凤凥処凧凨凩凪凫凬凭凮凯凰凱凲凳凴"
    u8"凵凶凷凸凹出击凼函凾凿刀刁刂刃刄刅分切刈刉刊刋刌刍刎刏刐刑划刓刔"
    u8"刕刖列刘则刚创刜初刞刟删刡刢刣判別刦刧刨利刪别刬刭刮刯到刱刲刳刴"
    u8"刵制刷券刹刺刻刼刽刾刿剀剁剂剃剄剅剆則剈剉削剋剌前剎剏剐剑剒剓剔"
    u8"剕剖剗剘剙剚剛剜剝剞剟剠剡剢剣剤剥剦剧剨剩剪剫剬剭剮副剰剱割剳剴"
    u8"創剶剷剸剹剺剻剼剽剾剿劀劁劂劃劄劅劆劇劈劉劊劋劌劍劎劏劐劑劒劓劔"
    u8"劕劖劗劘劙劚力劜劝办功加务劢劣劤劥劦劧动助努劫劬劭劮劯劰励劲劳労"
    u8"劵劶劷劸効劺劻劼劽劾势勀勁勂勃勄勅勆勇勈勉勊勋勌勍勎勏勐勑勒勓勔"
    u8"動勖勗勘務勚勛勜勝勞募勠勡勢勣勤勥勦勧勨勩勪勫勬勭勮勯勰勱勲勳勴"
    u8"勵勶勷勸勹勺勻勼勽勾勿匀匁匂匃匄包匆匇匈匉匊匋匌匍匎匏匐匑匒匓匔"
    u8"匕化北匘匙匚匛匜匝匞匟匠匡匢匣匤匥匦匧匨匩匪匫匬匭匮匯匰匱匲匳匴"
    u8"匵匶匷匸匹区医匼匽匾匿區十卂千卄卅卆升午卉半卋卌卍华协卐卑卒卓協"
    u8"单卖南単卙博卛卜卝卞卟占卡卢卣卤卥卦卧卨卩卪卫卬卭卮卯印危卲即却"
    u8"卵卶卷卸卹卺卻卼卽卾卿厀厁厂厃厄厅历厇厈厉厊压厌厍厎厏厐厑厒厓厔"
    u8"厕厖厗厘厙厚厛厜厝厞原厠厡厢厣厤厥厦厧厨厩厪厫厬厭厮厯厰厱厲厳厴"
    u8"厵厶厷厸厹厺去厼厽厾县叀叁参參叄叅叆叇又叉及友双反収叏叐发叒叓叔"
    u8"叕取受变叙叚叛叜叝叞叟叠叡叢口古句另叧叨叩只叫召叭叮可台叱史右叴"
    u8"叵叶号司叹叺叻叼叽叾叿吀吁吂吃各吅吆吇合吉吊吋同名后吏吐向吒吓吔"
    u8"吕吖吗吘吙吚君吜吝吞吟吠吡吢吣吤吥否吧吨吩吪含听吭吮启吰吱吲吳吴"
    u8"吵吶吷吸吹吺吻吼吽吾吿呀呁呂呃呄呅呆呇呈呉告呋呌呍呎呏呐呑呒呓呔"
    u8"呕呖呗员呙呚呛呜呝呞呟呠呡呢呣呤呥呦呧周呩呪呫呬呭呮呯呰呱呲味呴"
    u8"呵呶呷呸呹呺呻呼命呾呿咀咁咂咃咄咅咆咇咈咉咊咋和咍咎咏咐咑咒咓咔"
    u8"咕咖咗咘咙咚咛咜咝咞咟咠咡咢咣咤咥咦咧咨咩咪咫咬咭咮咯咰咱咲咳咴"
    u8"咵咶咷咸咹咺咻咼咽咾咿哀品哂哃哄哅哆哇哈哉哊哋哌响哎哏哐哑哒哓哔"
    u8"哕哖哗哘哙哚哛哜哝哞哟哠員哢哣哤哥哦哧哨哩哪哫哬哭哮哯哰哱哲哳哴"
    u8"哵哶哷哸哹哺哻哼哽哾哿唀唁唂唃唄唅唆唇唈唉唊唋唌唍唎唏唐唑唒唓唔"
    u8"唕唖唗唘唙唚唛唜唝唞唟唠唡唢唣唤唥唦唧唨唩唪唫唬唭售唯唰唱唲唳唴"
    u8"唵唶唷唸唹唺唻唼唽唾唿啀啁啂啃啄啅商啇啈啉啊啋啌啍啎問啐啑啒啓啔"
    u8"啕啖啗啘啙啚啛啜啝啞啟啠啡啢啣啤啥啦啧啨啩啪啫啬啭啮啯啰啱啲啳啴"
    u8"啵啶啷啸啹啺啻啼啽啾啿喀喁喂喃善喅喆喇喈喉喊喋喌喍喎喏喐喑喒喓喔"
    u8"喕喖喗喘喙喚喛喜喝喞喟喠喡喢喣喤喥喦喧喨喩喪喫喬喭單喯喰喱喲喳喴"
    u8"喵営喷喸喹喺喻喼喽喾喿嗀嗁嗂嗃嗄嗅嗆嗇嗈嗉嗊嗋嗌嗍嗎嗏嗐嗑嗒嗓嗔"
    u8"嗕嗖嗗嗘嗙嗚嗛嗜嗝嗞嗟嗠嗡嗢嗣嗤嗥嗦嗧嗨嗩嗪嗫嗬嗭嗮嗯嗰嗱嗲嗳嗴"
    u8"嗵嗶嗷嗸嗹嗺嗻嗼嗽嗾嗿嘀嘁嘂嘃嘄嘅嘆嘇嘈嘉嘊嘋嘌嘍嘎嘏嘐嘑嘒嘓嘔"
    u8"嘕嘖嘗嘘嘙嘚嘛嘜嘝嘞嘟嘠嘡嘢嘣嘤嘥嘦嘧嘨嘩嘪嘫嘬嘭嘮嘯嘰嘱嘲嘳嘴"
    u8"嘵嘶嘷嘸嘹嘺嘻嘼嘽嘾嘿噀噁噂噃噄噅噆噇噈噉噊噋噌噍噎噏噐噑噒噓噔"
    u8"噕噖噗噘噙噚噛噜噝噞噟噠噡噢噣噤噥噦噧器噩噪噫噬噭噮噯噰噱噲噳噴"
    u8"噵噶噷噸噹噺噻噼噽噾噿嚀嚁嚂嚃嚄嚅嚆嚇嚈嚉嚊嚋嚌嚍嚎嚏嚐嚑嚒嚓嚔"
    u8"嚕嚖嚗嚘嚙嚚嚛嚜嚝嚞嚟嚠嚡嚢嚣嚤嚥嚦嚧嚨嚩嚪嚫嚬嚭嚮嚯嚰嚱嚲嚳嚴"
    u8"嚵嚶嚷嚸嚹嚺嚻嚼嚽嚾嚿囀囁囂囃囄囅囆囇囈囉囊囋囌囍囎囏囐囑囒囓囔"
    u8"囕囖囗囘囙囚四囜囝回囟因囡团団囤囥囦囧囨囩囪囫囬园囮囯困囱囲図围"
    u8"囵囶囷囸囹固囻囼国图囿圀圁圂圃圄圅圆圇圈圉圊國圌圍圎圏圐圑園圓圔"
    u8"圕圖圗團圙圚圛圜圝圞土圠圡圢圣圤圥圦圧在圩圪圫圬圭圮圯地圱圲圳圴"
    u8"圵圶圷圸圹场圻圼圽圾圿址坁坂坃坄坅坆均坈坉坊坋坌坍坎坏坐坑坒坓坔"
    u8"坕坖块坘坙坚坛坜坝坞坟坠坡坢坣坤坥坦坧坨坩坪坫坬坭坮坯坰坱坲坳坴"
    u8"坵坶坷坸坹坺坻坼坽坾坿垀垁垂垃垄垅垆垇垈垉垊型垌垍垎垏垐垑垒垓垔"
    u8"垕垖垗垘垙垚垛垜垝垞垟垠垡垢垣垤垥垦垧垨垩垪垫垬垭垮垯垰垱垲垳垴"
    u8"垵垶垷垸垹垺垻垼垽垾垿埀埁埂埃埄埅埆埇埈埉埊埋埌埍城埏埐埑埒埓埔"
    u8"埕埖埗埘埙埚埛埜埝埞域埠埡埢埣埤埥埦埧埨埩埪埫埬埭埮埯埰埱埲埳埴"
    u8"埵埶執埸培基埻埼埽埾埿堀堁堂堃堄堅堆堇堈堉堊堋堌堍堎堏堐堑堒堓堔"
    u8"堕堖堗堘堙堚堛堜堝堞堟堠堡堢堣堤堥堦堧堨堩堪堫堬堭堮堯堰報堲堳場"
    u8"堵堶堷堸堹堺堻堼堽堾堿塀塁塂塃塄塅塆塇塈塉塊塋塌塍塎塏塐塑塒塓塔"
    u8"塕塖塗塘塙塚塛塜塝塞塟塠塡塢塣塤塥塦塧塨塩塪填塬塭塮塯塰塱塲塳塴"
    u8"塵塶塷塸塹塺塻塼塽塾塿墀墁墂境墄墅墆墇墈墉墊墋墌墍墎墏墐墑墒墓墔"
    u8"墕墖増墘墙墚墛墜墝增墟墠墡墢墣墤墥墦墧墨墩墪墫墬墭墮墯墰墱墲墳墴"
    u8"墵墶墷墸墹墺墻墼墽墾墿壀壁壂壃壄壅壆壇壈壉壊壋壌壍壎壏壐壑壒壓壔"
    u8"壕壖壗壘壙壚壛壜壝壞壟壠壡壢壣壤壥壦壧壨壩壪士壬壭壮壯声壱売壳壴"
    u8"壵壶壷壸壹壺壻壼壽壾壿夀夁夂夃处夅夆备夈変夊夋夌复夎夏夐夑夒夓夔"
    u8"夕外夗夘夙多夛夜夝夞够夠夡夢夣夤夥夦大夨天太夫夬夭央夯夰失夲夳头"
    u8"夵夶夷夸夹夺夻夼夽夾夿奀奁奂奃奄奅奆奇奈奉奊奋奌奍奎奏奐契奒奓奔"
    u8"奕奖套奘奙奚奛奜奝奞奟奠奡奢奣奤奥奦奧奨奩奪奫奬奭奮奯奰奱奲女奴"
    u8"奵奶奷奸她奺奻奼好奾奿妀妁如妃妄妅妆妇妈妉妊妋妌妍妎妏妐妑妒妓妔"
    u8"妕妖妗妘妙妚妛妜妝妞妟妠妡妢妣妤妥妦妧妨妩妪妫妬妭妮妯妰妱妲妳妴"
    u8"妵妶妷妸妹妺妻妼妽妾妿姀姁姂姃姄姅姆姇姈姉姊始姌姍姎姏姐姑姒姓委"
    u8"姕姖姗姘姙姚姛姜姝姞姟姠姡姢姣姤姥姦姧姨姩姪姫姬姭姮姯姰姱姲姳姴"
    u8"姵姶姷姸姹姺姻姼姽姾姿娀威娂娃娄娅娆娇娈娉娊娋娌娍娎娏娐娑娒娓娔"
    u8"娕娖娗娘娙娚娛娜娝娞娟娠娡娢娣娤娥娦娧娨娩娪娫娬娭娮娯娰娱娲娳娴"
    u8"娵娶娷娸娹娺娻娼娽娾娿婀婁婂婃婄婅婆婇婈婉婊婋婌婍婎婏婐婑婒婓婔"
    u8"婕婖婗婘婙婚婛婜婝婞婟婠婡婢婣婤婥婦婧婨婩婪婫婬婭婮婯婰婱婲婳婴"
    u8"婵婶婷婸婹婺婻婼婽婾婿媀媁媂媃媄媅媆媇媈媉媊媋媌媍媎媏媐媑媒媓媔"
    u8"媕媖媗媘媙媚媛媜媝媞媟媠媡媢媣媤媥媦媧媨媩媪媫媬媭媮媯媰媱媲媳媴"
    u8"媵媶媷媸媹媺媻媼媽媾媿嫀嫁嫂嫃嫄嫅嫆嫇嫈嫉嫊嫋嫌嫍嫎嫏嫐嫑嫒嫓嫔"
    u8"嫕嫖嫗嫘嫙嫚嫛嫜嫝嫞嫟嫠嫡嫢嫣嫤嫥嫦嫧嫨嫩嫪嫫嫬嫭嫮嫯嫰嫱嫲嫳嫴"
    u8"嫵嫶嫷嫸嫹嫺嫻嫼嫽嫾嫿嬀嬁嬂嬃嬄嬅嬆嬇嬈嬉嬊嬋嬌嬍嬎嬏嬐嬑嬒嬓嬔"
    u8"嬕嬖嬗嬘嬙嬚嬛嬜嬝嬞嬟嬠嬡嬢嬣嬤嬥嬦嬧嬨嬩嬪嬫嬬嬭嬮嬯嬰嬱嬲嬳嬴"
    u8"嬵嬶嬷嬸嬹嬺嬻嬼嬽嬾嬿孀孁孂孃孄孅孆孇孈孉孊孋孌孍孎孏子孑孒孓孔"
    u8"孕孖字存孙孚孛孜孝孞孟孠孡孢季孤孥学孧孨孩孪孫孬孭孮孯孰孱孲孳孴"
    u8"孵孶孷學孹孺孻孼孽孾孿宀宁宂它宄宅宆宇守安宊宋完宍宎宏宐宑宒宓宔"
    u8"宕宖宗官宙定宛宜宝实実宠审客宣室宥宦宧宨宩宪宫宬宭宮宯宰宱宲害宴"
    u8"宵家宷宸容宺宻宼宽宾宿寀寁寂寃寄寅密寇寈寉寊寋富寍寎寏寐寑寒寓寔"
    u8"寕寖寗寘寙寚寛寜寝寞察寠寡寢寣寤寥實寧寨審寪寫寬寭寮寯寰寱寲寳寴"
    u8"寵寶寷寸对寺寻导寽対寿尀封専尃射尅将將專尉尊尋尌對導小尐少尒尓尔"
    u8"尕尖尗尘尙尚尛尜尝尞尟尠尡尢尣尤尥尦尧尨尩尪尫尬尭尮尯尰就尲尳尴"
    u8"尵尶尷尸尹尺尻尼尽尾尿局屁层屃屄居屆屇屈屉届屋屌屍屎屏屐屑屒屓屔"
    u8"展屖屗屘屙屚屛屜屝属屟屠屡屢屣層履屦屧屨屩屪屫屬屭屮屯屰山屲屳屴"
    u8"屵屶屷屸屹屺屻屼屽屾屿岀岁岂岃岄岅岆岇岈岉岊岋岌岍岎岏岐岑岒岓岔"
    u8"岕岖岗岘岙岚岛岜岝岞岟岠岡岢岣岤岥岦岧岨岩岪岫岬岭岮岯岰岱岲岳岴"
    u8"岵岶岷岸岹岺岻岼岽岾岿峀峁峂峃峄峅峆峇峈峉峊峋峌峍峎峏峐峑峒峓峔"
    u8"峕峖峗峘峙峚峛峜峝峞峟峠峡峢峣峤峥峦峧峨峩峪峫峬峭峮峯峰峱峲峳峴"
    u8"峵島峷峸峹峺峻峼峽峾峿崀崁崂崃崄崅崆崇崈崉崊崋崌崍崎崏崐崑崒崓崔"
    u8"崕崖崗崘崙崚崛崜崝崞崟崠崡崢崣崤崥崦崧崨崩崪崫崬崭崮崯崰崱崲崳崴"
    u8"崵崶崷崸崹崺崻崼崽崾崿嵀嵁嵂嵃嵄嵅嵆嵇嵈嵉嵊嵋嵌嵍嵎嵏嵐嵑嵒嵓嵔"
    u8"嵕嵖嵗嵘嵙嵚嵛嵜嵝嵞嵟嵠嵡嵢嵣嵤嵥嵦嵧嵨嵩嵪嵫嵬嵭嵮嵯嵰嵱嵲嵳嵴"
    u8"嵵嵶嵷嵸嵹嵺嵻嵼嵽嵾嵿嶀嶁嶂嶃嶄嶅嶆嶇嶈嶉嶊嶋嶌嶍嶎嶏嶐嶑嶒嶓嶔"
    u8"嶕嶖嶗嶘嶙嶚嶛嶜嶝嶞嶟嶠嶡嶢嶣嶤嶥嶦嶧嶨嶩嶪嶫嶬嶭嶮嶯嶰嶱嶲嶳嶴"
    u8"嶵嶶嶷嶸嶹嶺嶻嶼嶽嶾嶿巀巁巂巃巄巅巆巇巈巉巊巋巌巍巎巏巐巑巒巓巔"
    u8"巕巖巗巘巙巚巛巜川州巟巠巡巢巣巤工左巧巨巩巪巫巬巭差巯巰己已巳巴"
    u8"巵巶巷巸巹巺巻巼巽巾巿帀币市布帄帅帆帇师帉帊帋希帍帎帏帐帑帒帓帔"
    u8"帕帖帗帘帙帚帛帜帝帞帟帠帡帢帣帤帥带帧帨帩帪師帬席帮帯帰帱帲帳帴"
    u8"帵帶帷常帹帺帻帼帽帾帿幀幁幂幃幄幅幆幇幈幉幊幋幌幍幎幏幐幑幒幓幔"
    u8"幕幖幗幘幙幚幛幜幝幞幟幠幡幢幣幤幥幦幧幨幩幪幫幬幭幮幯幰幱干平年"
    u8"幵并幷幸幹幺幻幼幽幾广庀庁庂広庄庅庆庇庈庉床庋庌庍庎序庐庑庒库应"
    u8"底庖店庘庙庚庛府庝庞废庠庡庢庣庤庥度座庨庩庪庫庬庭庮庯庰庱庲庳庴"
    u8"庵庶康庸庹庺庻庼庽庾庿廀廁廂廃廄廅廆廇廈廉廊廋廌廍廎廏廐廑廒廓廔"
    u8"廕廖廗廘廙廚廛廜廝廞廟廠廡廢廣廤廥廦廧廨廩廪廫廬廭廮廯廰廱廲廳廴"
    u8"廵延廷廸廹建廻廼廽廾廿开弁异弃弄弅弆弇弈弉弊弋弌弍弎式弐弑弒弓弔"
    u8"引弖弗弘弙弚弛弜弝弞弟张弡弢弣弤弥弦弧弨弩弪弫弬弭弮弯弰弱弲弳弴"
    u8"張弶強弸弹强弻弼弽弾弿彀彁彂彃彄彅彆彇彈彉彊彋彌彍彎彏彐彑归当彔"
    u8"录彖彗彘彙彚彛彜彝彞彟彠彡形彣彤彥彦彧彨彩彪彫彬彭彮彯彰影彲彳彴"
    u8"彵彶彷彸役彺彻彼彽彾彿往征徂徃径待徆徇很徉徊律後徍徎徏徐徑徒従徔"
    u8"徕徖得徘徙徚徛徜徝從徟徠御徢徣徤徥徦徧徨復循徫徬徭微徯徰徱徲徳徴"
    u8"徵徶德徸徹徺徻徼徽徾徿忀忁忂心忄必忆忇忈忉忊忋忌忍忎忏忐忑忒忓忔"
    u8"忕忖志忘忙忚忛応忝忞忟忠忡忢忣忤忥忦忧忨忩忪快忬忭忮忯忰忱忲忳忴"
    u8"念忶忷忸忹忺忻忼忽忾忿怀态怂怃怄怅怆怇怈怉怊怋怌怍怎怏怐怑怒怓怔"
    u8"怕怖怗怘怙怚怛怜思怞怟怠怡怢怣怤急怦性怨怩怪怫怬怭怮怯怰怱怲怳怴"
    u8"怵怶怷怸怹怺总怼怽怾怿恀恁恂恃恄恅恆恇恈恉恊恋恌恍恎恏恐恑恒恓恔"
    u8"恕恖恗恘恙恚恛恜恝恞恟恠恡恢恣恤恥恦恧恨恩恪恫恬恭恮息恰恱恲恳恴"
    u8"恵恶恷恸恹恺恻恼恽恾恿悀悁悂悃悄悅悆悇悈悉悊悋悌悍悎悏悐悑悒悓悔"
    u8"悕悖悗悘悙悚悛悜悝悞悟悠悡悢患悤悥悦悧您悩悪悫悬悭悮悯悰悱悲悳悴"
    u8"悵悶悷悸悹悺悻悼悽悾悿惀惁惂惃惄情惆惇惈惉惊惋惌惍惎惏惐惑惒惓惔"
    u8"惕惖惗惘惙惚惛惜惝惞惟惠惡惢惣惤惥惦惧惨惩惪惫惬惭惮惯惰惱惲想惴"
    u8"惵惶惷惸惹惺惻惼惽惾惿愀愁愂愃愄愅愆愇愈愉愊愋愌愍愎意愐愑愒愓愔"
    u8"愕愖愗愘愙愚愛愜愝愞感愠愡愢愣愤愥愦愧愨愩愪愫愬愭愮愯愰愱愲愳愴"
    u8"愵愶愷愸愹愺愻愼愽愾愿慀慁慂慃慄慅慆慇慈慉慊態慌慍慎慏慐慑慒慓慔"
    u8"慕慖慗慘慙慚慛慜慝慞慟慠慡慢慣慤慥慦慧慨慩慪慫慬慭慮慯慰慱慲慳慴"
    u8"慵慶慷慸慹慺慻慼慽慾慿憀憁憂憃憄憅憆憇憈憉憊憋憌憍憎憏憐憑憒憓憔"
    u8"憕憖憗憘憙憚憛憜憝憞憟憠憡憢憣憤憥憦憧憨憩憪憫憬憭憮憯憰憱憲憳憴"
    u8"憵憶憷憸憹憺憻憼憽憾憿懀懁懂懃懄懅懆懇懈應懊懋懌懍懎懏懐懑懒懓懔"
    u8"懕懖懗懘懙懚懛懜懝懞懟懠懡懢懣懤懥懦懧懨懩懪懫懬懭懮懯懰懱懲懳懴"
    u8"懵懶懷懸懹懺懻懼懽懾懿戀戁戂戃戄戅戆戇戈戉戊戋戌戍戎戏成我戒戓戔"
    u8"戕或戗战戙戚戛戜戝戞戟戠戡戢戣戤戥戦戧戨戩截戫戬戭戮戯戰戱戲戳戴"
    u8"戵戶户戸戹戺戻戼戽戾房所扁扂扃扄扅扆扇扈扉扊手扌才扎扏扐扑扒打扔"
    u8"払扖扗托扙扚扛扜扝扞扟扠扡扢扣扤扥扦执扨扩扪扫扬扭扮扯扰扱扲扳扴"
    u8"扵扶扷扸批扺扻扼扽找承技抁抂抃抄抅抆抇抈抉把抋抌抍抎抏抐抑抒抓抔"
    u8"投抖抗折抙抚抛抜抝択抟抠抡抢抣护报抦抧抨抩抪披抬抭抮抯抰抱抲抳抴"
    u8"抵抶抷抸抹抺抻押抽抾抿拀拁拂拃拄担拆拇拈拉拊拋拌拍拎拏拐拑拒拓拔"
    u8"拕拖拗拘拙拚招拜拝拞拟拠拡拢拣拤拥拦拧拨择拪拫括拭拮拯拰拱拲拳拴"
    u8"拵拶拷拸拹拺拻拼拽拾拿挀持挂挃挄挅挆指挈按挊挋挌挍挎挏挐挑挒挓挔"
    u8"挕挖挗挘挙挚挛挜挝挞挟挠挡挢挣挤挥挦挧挨挩挪挫挬挭挮振挰挱挲挳挴"
    u8"挵挶挷挸挹挺挻挼挽挾挿捀捁捂捃捄捅捆捇捈捉捊捋捌捍捎捏捐捑捒捓捔"
    u8"捕捖捗捘捙捚捛捜捝捞损捠捡换捣捤捥捦捧捨捩捪捫捬捭据捯捰捱捲捳捴"
    u8"捵捶捷捸捹捺捻捼捽捾捿掀掁掂掃掄掅掆掇授掉掊掋掌掍掎掏掐掑排掓掔"
    u8"掕掖掗掘掙掚掛掜掝掞掟掠採探掣掤接掦控推掩措掫掬掭掮掯掰掱掲掳掴"
    u8"掵掶掷掸掹掺掻掼掽掾掿揀揁揂揃揄揅揆揇揈揉揊揋揌揍揎描提揑插揓揔"
    u8"揕揖揗揘揙揚換揜揝揞揟揠握揢揣揤揥揦揧揨揩揪揫揬揭揮揯揰揱揲揳援"
    u8"揵揶揷揸揹揺揻揼揽揾揿搀搁搂搃搄搅搆搇搈搉搊搋搌損搎搏搐搑搒搓搔"
    u8"搕搖搗搘搙搚搛搜搝搞搟搠搡搢搣搤搥搦搧搨搩搪搫搬搭搮搯搰搱搲搳搴"
    u8"搵搶搷搸搹携搻搼搽搾搿摀摁摂摃摄摅摆摇摈摉摊摋摌摍摎摏摐摑摒摓摔"
    u8"摕摖摗摘摙摚摛摜摝摞摟摠摡摢摣摤摥摦摧摨摩摪摫摬摭摮摯摰摱摲摳摴"
    u8"摵摶摷摸摹摺摻摼摽摾摿撀撁撂撃撄撅撆撇撈撉撊撋撌撍撎撏撐撑撒撓撔"
    u8"撕撖撗撘撙撚撛撜撝撞撟撠撡撢撣撤撥撦撧撨撩撪撫撬播撮撯撰撱撲撳撴"
    u8"撵撶撷撸撹撺撻撼撽撾撿擀擁擂擃擄擅擆擇擈擉擊擋擌操擎擏擐擑擒擓擔"
    u8"擕擖擗擘擙據擛擜擝擞擟擠擡擢擣擤擥擦擧擨擩擪擫擬擭擮擯擰擱擲擳擴"
    u8"擵擶擷擸擹擺擻擼擽擾擿攀攁攂攃攄攅攆攇攈攉攊攋攌攍攎攏攐攑攒攓攔"
    u8"攕攖攗攘攙攚攛攜攝攞攟攠攡攢攣攤攥攦攧攨攩攪攫攬攭攮支攰攱攲攳攴"
    u8"攵收攷攸改攺攻攼攽放政敀敁敂敃敄故敆敇效敉敊敋敌敍敎敏敐救敒敓敔"
    u8"敕敖敗敘教敚敛敜敝敞敟敠敡敢散敤敥敦敧敨敩敪敫敬敭敮敯数敱敲敳整"
    u8"敵敶敷數敹敺敻敼敽敾敿斀斁斂斃斄斅斆文斈斉斊斋斌斍斎斏斐斑斒斓斔"
    u8"斕斖斗斘料斚斛斜斝斞斟斠斡斢斣斤斥斦斧斨斩斪斫斬断斮斯新斱斲斳斴"
    u8"斵斶斷斸方斺斻於施斾斿旀旁旂旃旄旅旆旇旈旉旊旋旌旍旎族旐旑旒旓旔"
    u8"旕旖旗旘旙旚旛旜旝旞旟无旡既旣旤日旦旧旨早旪旫旬旭旮旯旰旱旲旳旴"
    u8"旵时旷旸旹旺旻旼旽旾旿昀昁昂昃昄昅昆昇昈昉昊昋昌昍明昏昐昑昒易昔"
    u8"昕昖昗昘昙昚昛昜昝昞星映昡昢昣昤春昦昧昨昩昪昫昬昭昮是昰昱昲昳昴"
    u8"昵昶昷昸昹昺昻昼昽显昿晀晁時晃晄晅晆晇晈晉晊晋晌晍晎晏晐晑晒晓晔"
    u8"晕晖晗晘晙晚晛晜晝晞晟晠晡晢晣晤晥晦晧晨晩晪晫晬晭普景晰晱晲晳晴"
    u8"晵晶晷晸晹智晻晼晽晾晿暀暁暂暃暄暅暆暇暈暉暊暋暌暍暎暏暐暑暒暓暔"
    u8"暕暖暗暘暙暚暛暜暝暞暟暠暡暢暣暤暥暦暧暨暩暪暫暬暭暮暯暰暱暲暳暴"
    u8"暵暶暷暸暹暺暻暼暽暾暿曀曁曂曃曄曅曆曇曈曉曊曋曌曍曎曏曐曑曒曓曔"
    u8"曕曖曗曘曙曚曛曜曝曞曟曠曡曢曣曤曥曦曧曨曩曪曫曬曭曮曯曰曱曲曳更"
    u8"曵曶曷書曹曺曻曼曽曾替最朁朂會朄朅朆朇月有朊朋朌服朎朏朐朑朒朓朔"
    u8"朕朖朗朘朙朚望朜朝朞期朠朡朢朣朤朥朦朧木朩未末本札朮术朰朱朲朳朴"
    u8"朵朶朷朸朹机朻朼朽朾朿杀杁杂权杄杅杆杇杈杉杊杋杌杍李杏材村杒杓杔"
    u8"杕杖杗杘杙杚杛杜杝杞束杠条杢杣杤来杦杧杨杩杪杫杬杭杮杯杰東杲杳杴"
    u8"杵杶杷杸杹杺杻杼杽松板枀极枂枃构枅枆枇枈枉枊枋枌枍枎枏析枑枒枓枔"
    u8"枕枖林枘枙枚枛果枝枞枟枠枡枢枣枤枥枦枧枨枩枪枫枬枭枮枯枰枱枲枳枴"
    u8"枵架枷枸枹枺枻枼枽枾枿柀柁柂柃柄柅柆柇柈柉柊柋柌柍柎柏某柑柒染柔"
    u8"柕柖柗柘柙柚柛柜柝柞柟柠柡柢柣柤查柦柧柨柩柪柫柬柭柮柯柰柱柲柳柴"
    u8"柵柶柷柸柹柺査柼柽柾柿栀栁栂栃栄栅栆标栈栉栊栋栌栍栎栏栐树栒栓栔"
    u8"栕栖栗栘栙栚栛栜栝栞栟栠校栢栣栤栥栦栧栨栩株栫栬栭栮栯栰栱栲栳栴"
    u8"栵栶样核根栺栻格栽栾栿桀桁桂桃桄桅框桇案桉桊桋桌桍桎桏桐桑桒桓桔"
    u8"桕桖桗桘桙桚桛桜桝桞桟桠桡桢档桤桥桦桧桨桩桪桫桬桭桮桯桰桱桲桳桴"
    u8"桵桶桷桸桹桺桻桼桽桾桿梀梁梂梃梄梅梆梇梈梉梊梋梌梍梎梏梐梑梒梓梔"
    u8"梕梖梗梘梙梚梛梜條梞梟梠梡梢梣梤梥梦梧梨梩梪梫梬梭梮梯械梱梲梳梴"
    u8"梵梶梷梸梹梺梻梼梽梾梿检棁棂棃棄棅棆棇棈棉棊棋棌棍棎棏棐棑棒棓棔"
    u8"棕棖棗棘棙棚棛棜棝棞棟棠棡棢棣棤棥棦棧棨棩棪棫棬棭森棯棰棱棲棳棴"
    u8"棵棶棷棸棹棺棻棼棽棾棿椀椁椂椃椄椅椆椇椈椉椊椋椌植椎椏椐椑椒椓椔"
    u8"椕椖椗椘椙椚椛検椝椞椟椠椡椢椣椤椥椦椧椨椩椪椫椬椭椮椯椰椱椲椳椴"
    u8"椵椶椷椸椹椺椻椼椽椾椿楀楁楂楃楄楅楆楇楈楉楊楋楌楍楎楏楐楑楒楓楔"
    u8"楕楖楗楘楙楚楛楜楝楞楟楠楡楢楣楤楥楦楧楨楩楪楫楬業楮楯楰楱楲楳楴"
    u8"極楶楷楸楹楺楻楼楽楾楿榀榁概榃榄榅榆榇榈榉榊榋榌榍榎榏榐榑榒榓榔"
    u8"榕榖榗榘榙榚榛榜榝榞榟榠榡榢榣榤榥榦榧榨榩榪榫榬榭榮榯榰榱榲榳榴"
    u8"榵榶榷榸榹榺榻榼榽榾榿槀槁槂槃槄槅槆槇槈槉槊構槌槍槎槏槐槑槒槓槔"
    u8"槕槖槗様槙槚槛槜槝槞槟槠槡槢槣槤槥槦槧槨槩槪槫槬槭槮槯槰槱槲槳槴"
    u8"槵槶槷槸槹槺槻槼槽槾槿樀樁樂樃樄樅樆樇樈樉樊樋樌樍樎樏樐樑樒樓樔"
    u8"樕樖樗樘標樚樛樜樝樞樟樠模樢樣樤樥樦樧樨権横樫樬樭樮樯樰樱樲樳樴"
    u8"樵樶樷樸樹樺樻樼樽樾樿橀橁橂橃橄橅橆橇橈橉橊橋橌橍橎橏橐橑橒橓橔"
    u8"橕橖橗橘橙橚橛橜橝橞機橠橡橢橣橤橥橦橧橨橩橪橫橬橭橮橯橰橱橲橳橴"
    u8"橵橶橷橸橹橺橻橼橽橾橿檀檁檂檃檄檅檆檇檈檉檊檋檌檍檎檏檐檑檒檓檔"
    u8"檕檖檗檘檙檚檛檜檝檞檟檠檡檢檣檤檥檦檧檨檩檪檫檬檭檮檯檰檱檲檳檴"
    u8"檵檶檷檸檹檺檻檼檽檾檿櫀櫁櫂櫃櫄櫅櫆櫇櫈櫉櫊櫋櫌櫍櫎櫏櫐櫑櫒櫓櫔"
    u8"櫕櫖櫗櫘櫙櫚櫛櫜櫝櫞櫟櫠櫡櫢櫣櫤櫥櫦櫧櫨櫩櫪櫫櫬櫭櫮櫯櫰櫱櫲櫳櫴"
    u8"櫵櫶櫷櫸櫹櫺櫻櫼櫽櫾櫿欀欁欂欃欄欅欆欇欈欉權欋欌欍欎欏欐欑欒欓欔"
    u8"欕欖欗欘欙欚欛欜欝欞欟欠次欢欣欤欥欦欧欨欩欪欫欬欭欮欯欰欱欲欳欴"
    u8"欵欶欷欸欹欺欻欼欽款欿歀歁歂歃歄歅歆歇歈歉歊歋歌歍歎歏歐歑歒歓歔"
    u8"歕歖歗歘歙歚歛歜歝歞歟歠歡止正此步武歧歨歩歪歫歬歭歮歯歰歱歲歳歴"
    u8"歵歶歷歸歹歺死歼歽歾歿殀殁殂殃殄殅殆殇殈殉殊残殌殍殎殏殐殑殒殓殔"
    u8"殕殖殗殘殙殚殛殜殝殞殟殠殡殢殣殤殥殦殧殨殩殪殫殬殭殮殯殰殱殲殳殴"
    u8"段殶殷殸殹殺殻殼殽殾殿毀毁毂毃毄毅毆毇毈毉毊毋毌母毎每毐毑毒毓比"
    u8"毕毖毗毘毙毚毛毜毝毞毟毠毡毢毣毤毥毦毧毨毩毪毫毬毭毮毯毰毱毲毳毴"
    u8"毵毶毷毸毹毺毻毼毽毾毿氀氁氂氃氄氅氆氇氈氉氊氋氌氍氎氏氐民氒氓气"
    u8"氕氖気氘氙氚氛氜氝氞氟氠氡氢氣氤氥氦氧氨氩氪氫氬氭氮氯氰氱氲氳水"
    u8"氵氶氷永氹氺氻氼氽氾氿汀汁求汃汄汅汆汇汈汉汊汋汌汍汎汏汐汑汒汓汔"
    u8"汕汖汗汘汙汚汛汜汝汞江池污汢汣汤汥汦汧汨汩汪汫汬汭汮汯汰汱汲汳汴"
    u8"汵汶汷汸汹決汻汼汽汾汿沀沁沂沃沄沅沆沇沈沉沊沋沌沍沎沏沐沑沒沓沔"
    u8"沕沖沗沘沙沚沛沜沝沞沟沠没沢沣沤沥沦沧沨沩沪沫沬沭沮沯沰沱沲河沴"
    u8"沵沶沷沸油沺治沼沽沾沿泀況泂泃泄泅泆泇泈泉泊泋泌泍泎泏泐泑泒泓泔"
    u8"法泖泗泘泙泚泛泜泝泞泟泠泡波泣泤泥泦泧注泩泪泫泬泭泮泯泰泱泲泳泴"
    u8"泵泶泷泸泹泺泻泼泽泾泿洀洁洂洃洄洅洆洇洈洉洊洋洌洍洎洏洐洑洒洓洔"
    u8"洕洖洗洘洙洚洛洜洝洞洟洠洡洢洣洤津洦洧洨洩洪洫洬洭洮洯洰洱洲洳洴"
    u8"洵洶洷洸洹洺活洼洽派洿浀流浂浃浄浅浆浇浈浉浊测浌浍济浏浐浑浒浓浔"
    u8"浕浖浗浘浙浚浛浜浝浞浟浠浡浢浣浤浥浦浧浨浩浪浫浬浭浮浯浰浱浲浳浴"
    u8"浵浶海浸浹浺浻浼浽浾浿涀涁涂涃涄涅涆涇消涉涊涋涌涍涎涏涐涑涒涓涔"
    u8"涕涖涗涘涙涚涛涜涝涞涟涠涡涢涣涤涥润涧涨涩涪涫涬涭涮涯涰涱液涳涴"
    u8"涵涶涷涸涹涺涻涼涽涾涿淀淁淂淃淄淅淆淇淈淉淊淋淌淍淎淏淐淑淒淓淔"
    u8"淕淖淗淘淙淚淛淜淝淞淟淠淡淢淣淤淥淦淧淨淩淪淫淬淭淮淯淰深淲淳淴"
    u8"淵淶混淸淹淺添淼淽淾淿渀渁渂渃渄清渆渇済渉渊渋渌渍渎渏渐渑渒渓渔"
    u8"渕渖渗渘渙渚減渜渝渞渟渠渡渢渣渤渥渦渧渨温渪渫測渭渮港渰渱渲渳渴"
    u8"渵渶渷游渹渺渻渼渽渾渿湀湁湂湃湄湅湆湇湈湉湊湋湌湍湎湏湐湑湒湓湔"
    u8"湕湖湗湘湙湚湛湜湝湞湟湠湡湢湣湤湥湦湧湨湩湪湫湬湭湮湯湰湱湲湳湴"
    u8"湵湶湷湸湹湺湻湼湽湾湿満溁溂溃溄溅溆溇溈溉溊溋溌溍溎溏源溑溒溓溔"
    u8"溕準溗溘溙溚溛溜溝溞溟溠溡溢溣溤溥溦溧溨溩溪溫溬溭溮溯溰溱溲溳溴"
    u8"溵溶溷溸溹溺溻溼溽溾溿滀滁滂滃滄滅滆滇滈滉滊滋滌滍滎滏滐滑滒滓滔"
    u8"滕滖滗滘滙滚滛滜滝滞滟滠满滢滣滤滥滦滧滨滩滪滫滬滭滮滯滰滱滲滳滴"
    u8"滵滶滷滸滹滺滻滼滽滾滿漀漁漂漃漄漅漆漇漈漉漊漋漌漍漎漏漐漑漒漓演"
    u8"漕漖漗漘漙漚漛漜漝漞漟漠漡漢漣漤漥漦漧漨漩漪漫漬漭漮漯漰漱漲漳漴"
    u8"漵漶漷漸漹漺漻漼漽漾漿潀潁潂潃潄潅潆潇潈潉潊潋潌潍潎潏潐潑潒潓潔"
    u8"潕潖潗潘潙潚潛潜潝潞潟潠潡潢潣潤潥潦潧潨潩潪潫潬潭潮潯潰潱潲潳潴"
    u8"潵潶潷潸潹潺潻潼潽潾潿澀澁澂澃澄澅澆澇澈澉澊澋澌澍澎澏澐澑澒澓澔"
    u8"澕澖澗澘澙澚澛澜澝澞澟澠澡澢澣澤澥澦澧澨澩澪澫澬澭澮澯澰澱澲澳澴"
    u8"澵澶澷澸澹澺澻澼澽澾澿激濁濂濃濄濅濆濇濈濉濊濋濌濍濎濏濐濑濒濓濔"
    u8"濕濖濗濘濙濚濛濜濝濞濟濠濡濢濣濤濥濦濧濨濩濪濫濬濭濮濯濰濱濲濳濴"
    u8"濵濶濷濸濹濺濻濼濽濾濿瀀瀁瀂瀃瀄瀅瀆瀇瀈瀉瀊瀋瀌瀍瀎瀏瀐瀑瀒瀓瀔"
    u8"瀕瀖瀗瀘瀙瀚瀛瀜瀝瀞瀟瀠瀡瀢瀣瀤瀥瀦瀧瀨瀩瀪瀫瀬瀭瀮瀯瀰瀱瀲瀳瀴"
    u8"瀵瀶瀷瀸瀹瀺瀻瀼瀽瀾瀿灀灁灂灃灄灅灆灇灈灉灊灋灌灍灎灏灐灑灒灓灔"
    u8"灕灖灗灘灙灚灛灜灝灞灟灠灡灢灣灤灥灦灧灨灩灪火灬灭灮灯灰灱灲灳灴"
    u8"灵灶灷灸灹灺灻灼災灾灿炀炁炂炃炄炅炆炇炈炉炊炋炌炍炎炏炐炑炒炓炔"
    u8"炕炖炗炘炙炚炛炜炝炞炟炠炡炢炣炤炥炦炧炨炩炪炫炬炭炮炯炰炱炲炳炴"
    u8"炵炶炷炸点為炻炼炽炾炿烀烁烂烃烄烅烆烇烈烉烊烋烌烍烎烏烐烑烒烓烔"
    u8"烕烖烗烘烙烚烛烜烝烞烟烠烡烢烣烤烥烦烧烨烩烪烫烬热烮烯烰烱烲烳烴"
    u8"烵烶烷烸烹烺烻烼烽烾烿焀焁焂焃焄焅焆焇焈焉焊焋焌焍焎焏焐焑焒焓焔"
    u8"焕焖焗焘焙焚焛焜焝焞焟焠無焢焣焤焥焦焧焨焩焪焫焬焭焮焯焰焱焲焳焴"
    u8"焵然焷焸焹焺焻焼焽焾焿煀煁煂煃煄煅煆煇煈煉煊煋煌煍煎煏煐煑煒煓煔"
    u8"煕煖煗煘煙煚煛煜煝煞煟煠煡煢煣煤煥煦照煨煩煪煫煬煭煮煯煰煱煲煳煴"
    u8"煵煶煷煸煹煺煻煼煽煾煿熀熁熂熃熄熅熆熇熈熉熊熋熌熍熎熏熐熑熒熓熔"
    u8"熕熖熗熘熙熚熛熜熝熞熟熠熡熢熣熤熥熦熧熨熩熪熫熬熭熮熯熰熱熲熳熴"
    u8"熵熶熷熸熹熺熻熼熽熾熿燀燁燂燃燄燅燆燇燈燉燊燋燌燍燎燏燐燑燒燓燔"
    u8"燕燖燗燘燙燚燛燜燝燞營燠燡燢燣燤燥燦燧燨燩燪燫燬燭燮燯燰燱燲燳燴"
    u8"燵燶燷燸燹燺燻燼燽燾燿爀爁爂爃爄爅爆爇爈爉爊爋爌爍爎爏爐爑爒爓爔"
    u8"爕爖爗爘爙爚爛爜爝爞爟爠爡爢爣爤爥爦爧爨爩爪爫爬爭爮爯爰爱爲爳爴"
    u8"爵父爷爸爹爺爻爼爽爾爿牀牁牂牃牄牅牆片版牉牊牋牌牍牎牏牐牑牒牓牔"
    u8"牕牖牗牘牙牚牛牜牝牞牟牠牡牢牣牤牥牦牧牨物牪牫牬牭牮牯牰牱牲牳牴"
    u8"牵牶牷牸特牺牻牼牽牾牿犀犁犂犃犄犅犆犇犈犉犊犋犌犍犎犏犐犑犒犓犔"
    u8"犕犖犗犘犙犚犛犜犝犞犟犠犡犢犣犤犥犦犧犨犩犪犫犬犭犮犯犰犱犲犳犴"
    u8"犵状犷犸犹犺犻犼犽犾犿狀狁狂狃狄狅狆狇狈狉狊狋狌狍狎狏狐狑狒狓狔"
    u8"狕狖狗狘狙狚狛狜狝狞狟狠狡狢狣狤狥狦狧狨狩狪狫独狭狮狯狰狱狲狳狴"
    u8"狵狶狷狸狹狺狻狼狽狾狿猀猁猂猃猄猅猆猇猈猉猊猋猌猍猎猏猐猑猒猓猔"
    u8"猕猖猗猘猙猚猛猜猝猞猟猠猡猢猣猤猥猦猧猨猩猪猫猬猭献猯猰猱猲猳猴"
    u8"猵猶猷猸猹猺猻猼猽猾猿獀獁獂獃獄獅獆獇獈獉獊獋獌獍獎獏獐獑獒獓獔"
    u8"獕獖獗獘獙獚獛獜獝獞獟獠獡獢獣獤獥獦獧獨獩獪獫獬獭獮獯獰獱獲獳獴"
    u8"獵獶獷獸獹獺獻獼獽獾獿玀玁玂玃玄玅玆率玈玉玊王玌玍玎玏玐玑玒玓玔"
    u8"玕玖玗玘玙玚玛玜玝玞玟玠玡玢玣玤玥玦玧玨玩玪玫玬玭玮环现玱玲玳玴"
    u8"玵玶玷玸玹玺玻玼玽玾玿珀珁珂珃珄珅珆珇珈珉珊珋珌珍珎珏珐珑珒珓珔"
    u8"珕珖珗珘珙珚珛珜珝珞珟珠珡珢珣珤珥珦珧珨珩珪珫珬班珮珯珰珱珲珳珴"
    u8"珵珶珷珸珹珺珻珼珽現珿琀琁琂球琄琅理琇琈琉琊琋琌琍琎琏琐琑琒琓琔"
    u8"琕琖琗琘琙琚琛琜琝琞琟琠琡琢琣琤琥琦琧琨琩琪琫琬琭琮琯琰琱琲琳琴"
    u8"琵琶琷琸琹琺琻琼琽琾琿瑀瑁瑂瑃瑄瑅瑆瑇瑈瑉瑊瑋瑌瑍瑎瑏瑐瑑瑒瑓瑔"
    u8"瑕瑖瑗瑘瑙瑚瑛瑜瑝瑞瑟瑠瑡瑢瑣瑤瑥瑦瑧瑨瑩瑪瑫瑬瑭瑮瑯瑰瑱瑲瑳瑴"
    u8"瑵瑶瑷瑸瑹瑺瑻瑼瑽瑾瑿璀璁璂璃璄璅璆璇璈璉璊璋璌璍璎璏璐璑璒璓璔"
    u8"璕璖璗璘璙璚璛璜璝璞璟璠璡璢璣璤璥璦璧璨璩璪璫璬璭璮璯環璱璲璳璴"
    u8"璵璶璷璸璹璺璻璼璽璾璿瓀瓁瓂瓃瓄瓅瓆瓇瓈瓉瓊瓋瓌瓍瓎瓏瓐瓑瓒瓓瓔"
    u8"瓕瓖瓗瓘瓙瓚瓛瓜瓝瓞瓟瓠瓡瓢瓣瓤瓥瓦瓧瓨瓩瓪瓫瓬瓭瓮瓯瓰瓱瓲瓳瓴"
    u8"瓵瓶瓷瓸瓹瓺瓻瓼瓽瓾瓿甀甁甂甃甄甅甆甇甈甉甊甋甌甍甎甏甐甑甒甓甔"
    u8"甕甖甗甘甙甚甛甜甝甞生甠甡產産甤甥甦甧用甩甪甫甬甭甮甯田由甲申甴"
    u8"电甶男甸甹町画甼甽甾甿畀畁畂畃畄畅畆畇畈畉畊畋界畍畎畏畐畑畒畓畔"
    u8"畕畖畗畘留畚畛畜畝畞畟畠畡畢畣畤略畦畧畨畩番畫畬畭畮畯異畱畲畳畴"
    u8"畵當畷畸畹畺畻畼畽畾畿疀疁疂疃疄疅疆疇疈疉疊疋疌疍疎疏疐疑疒疓疔"
    u8"疕疖疗疘疙疚疛疜疝疞疟疠疡疢疣疤疥疦疧疨疩疪疫疬疭疮疯疰疱疲疳疴"
    u8"疵疶疷疸疹疺疻疼疽疾疿痀痁痂痃痄病痆症痈痉痊痋痌痍痎痏痐痑痒痓痔"
    u8"痕痖痗痘痙痚痛痜痝痞痟痠痡痢痣痤痥痦痧痨痩痪痫痬痭痮痯痰痱痲痳痴"
    u8"痵痶痷痸痹痺痻痼痽痾痿瘀瘁瘂瘃瘄瘅瘆瘇瘈瘉瘊瘋瘌瘍瘎瘏瘐瘑瘒瘓瘔"
    u8"瘕瘖瘗瘘瘙瘚瘛瘜瘝瘞瘟瘠瘡瘢瘣瘤瘥瘦瘧瘨瘩瘪瘫瘬瘭瘮瘯瘰瘱瘲瘳瘴"
    u8"瘵瘶瘷瘸瘹瘺瘻瘼瘽瘾瘿癀癁療癃癄癅癆癇癈癉癊癋癌癍癎癏癐癑癒癓癔"
    u8"癕癖癗癘癙癚癛癜癝癞癟癠癡癢癣癤癥癦癧癨癩癪癫癬癭癮癯癰癱癲癳癴"
    u8"癵癶癷癸癹発登發白百癿皀皁皂皃的皅皆皇皈皉皊皋皌皍皎皏皐皑皒皓皔"
    u8"皕皖皗皘皙皚皛皜皝皞皟皠皡皢皣皤皥皦皧皨皩皪皫皬皭皮皯皰皱皲皳皴"
    u8"皵皶皷皸皹皺皻皼皽皾皿盀盁盂盃盄盅盆盇盈盉益盋盌盍盎盏盐监盒盓盔"
    u8"盕盖盗盘盙盚盛盜盝盞盟盠盡盢監盤盥盦盧盨盩盪盫盬盭目盯盰盱盲盳直"
    u8"盵盶盷相盹盺盻盼盽盾盿眀省眂眃眄眅眆眇眈眉眊看県眍眎眏眐眑眒眓眔"
    u8"眕眖眗眘眙眚眛眜眝眞真眠眡眢眣眤眥眦眧眨眩眪眫眬眭眮眯眰眱眲眳眴"
    u8"眵眶眷眸眹眺眻眼眽眾眿着睁睂睃睄睅睆睇睈睉睊睋睌睍睎睏睐睑睒睓睔"
    u8"睕睖睗睘睙睚睛睜睝睞睟睠睡睢督睤睥睦睧睨睩睪睫睬睭睮睯睰睱睲睳睴"
    u8"睵睶睷睸睹睺睻睼睽睾睿瞀瞁瞂瞃瞄瞅瞆瞇瞈瞉瞊瞋瞌瞍瞎瞏瞐瞑瞒瞓瞔"
    u8"瞕瞖瞗瞘瞙瞚瞛瞜瞝瞞瞟瞠瞡瞢瞣瞤瞥瞦瞧瞨瞩瞪瞫瞬瞭瞮瞯瞰瞱瞲瞳瞴"
    u8"瞵瞶瞷瞸瞹瞺瞻瞼瞽瞾瞿矀矁矂矃矄矅矆矇矈矉矊矋矌矍矎矏矐矑矒矓矔"
    u8"矕矖矗矘矙矚矛矜矝矞矟矠矡矢矣矤知矦矧矨矩矪矫矬短矮矯矰矱矲石矴"
    u8"矵矶矷矸矹矺矻矼矽矾矿砀码砂砃砄砅砆砇砈砉砊砋砌砍砎砏砐砑砒砓研"
    u8"砕砖砗砘砙砚砛砜砝砞砟砠砡砢砣砤砥砦砧砨砩砪砫砬砭砮砯砰砱砲砳破"
    u8"砵砶砷砸砹砺砻砼砽砾砿础硁硂硃硄硅硆硇硈硉硊硋硌硍硎硏硐硑硒硓硔"
    u8"硕硖硗硘硙硚硛硜硝硞硟硠硡硢硣硤硥硦硧硨硩硪硫硬硭确硯硰硱硲硳硴"
    u8"硵硶硷硸硹硺硻硼硽硾硿碀碁碂碃碄碅碆碇碈碉碊碋碌碍碎碏碐碑碒碓碔"
    u8"碕碖碗碘碙碚碛碜碝碞碟碠碡碢碣碤碥碦碧碨碩碪碫碬碭碮碯碰碱碲碳碴"
    u8"碵碶碷碸碹確碻碼碽碾碿磀磁磂磃磄磅磆磇磈磉磊磋磌磍磎磏磐磑磒磓磔"
    u8"磕磖磗磘磙磚磛磜磝磞磟磠磡磢磣磤磥磦磧磨磩磪磫磬磭磮磯磰磱磲磳磴"
    u8"磵磶磷磸磹磺磻磼磽磾磿礀礁礂礃礄礅礆礇礈礉礊礋礌礍礎礏礐礑礒礓礔"
    u8"礕礖礗礘礙礚礛礜礝礞礟礠礡礢礣礤礥礦礧礨礩礪礫礬礭礮礯礰礱礲礳礴"
    u8"礵礶礷礸礹示礻礼礽社礿祀祁祂祃祄祅祆祇祈祉祊祋祌祍祎祏祐祑祒祓祔"
    u8"祕祖祗祘祙祚祛祜祝神祟祠祡祢祣祤祥祦祧票祩祪祫祬祭祮祯祰祱祲祳祴"
    u8"祵祶祷祸祹祺祻祼祽祾祿禀禁禂禃禄禅禆禇禈禉禊禋禌禍禎福禐禑禒禓禔"
    u8"禕禖禗禘禙禚禛禜禝禞禟禠禡禢禣禤禥禦禧禨禩禪禫禬禭禮禯禰禱禲禳禴"
    u8"禵禶禷禸禹禺离禼禽禾禿秀私秂秃秄秅秆秇秈秉秊秋秌种秎秏秐科秒秓秔"
    u8"秕秖秗秘秙秚秛秜秝秞租秠秡秢秣秤秥秦秧秨秩秪秫秬秭秮积称秱秲秳秴"
    u8"秵秶秷秸秹秺移秼秽秾秿稀稁稂稃稄稅稆稇稈稉稊程稌稍税稏稐稑稒稓稔"
    u8"稕稖稗稘稙稚稛稜稝稞稟稠稡稢稣稤稥稦稧稨稩稪稫稬稭種稯稰稱稲稳稴"
    u8"稵稶稷稸稹稺稻稼稽稾稿穀穁穂穃穄穅穆穇穈穉穊穋穌積穎穏穐穑穒穓穔"
    u8"穕穖穗穘穙穚穛穜穝穞穟穠穡穢穣穤穥穦穧穨穩穪穫穬穭穮穯穰穱穲穳穴"
    u8"穵究穷穸穹空穻穼穽穾穿窀突窂窃窄窅窆窇窈窉窊窋窌窍窎窏窐窑窒窓窔"
    u8"窕窖窗窘窙窚窛窜窝窞窟窠窡窢窣窤窥窦窧窨窩窪窫窬窭窮窯窰窱窲窳窴"
    u8"窵窶窷窸窹窺窻窼窽窾窿竀竁竂竃竄竅竆竇竈竉竊立竌竍竎竏竐竑竒竓竔"
    u8"竕竖竗竘站竚竛竜竝竞竟章竡竢竣竤童竦竧竨竩竪竫竬竭竮端竰竱竲竳竴"
    u8"竵競竷竸竹竺竻竼竽竾竿笀笁笂笃笄笅笆笇笈笉笊笋笌笍笎笏笐笑笒笓笔"
    u8"笕笖笗笘笙笚笛笜笝笞笟笠笡笢笣笤笥符笧笨笩笪笫第笭笮笯笰笱笲笳笴"
    u8"笵笶笷笸笹笺笻笼笽笾笿筀筁筂筃筄筅筆筇筈等筊筋筌筍筎筏筐筑筒筓答"
    u8"筕策筗筘筙筚筛筜筝筞筟筠筡筢筣筤筥筦筧筨筩筪筫筬筭筮筯筰筱筲筳筴"
    u8"筵筶筷筸筹筺筻筼筽签筿简箁箂箃箄箅箆箇箈箉箊箋箌箍箎箏箐箑箒箓箔"
    u8"箕箖算箘箙箚箛箜箝箞箟箠管箢箣箤箥箦箧箨箩箪箫箬箭箮箯箰箱箲箳箴"
    u8"箵箶箷箸箹箺箻箼箽箾箿節篁篂篃範篅篆篇篈築篊篋篌篍篎篏篐篑篒篓篔"
    u8"篕篖篗篘篙篚篛篜篝篞篟篠篡篢篣篤篥篦篧篨篩篪篫篬篭篮篯篰篱篲篳篴"
    u8"篵篶篷篸篹篺篻篼篽篾篿簀簁簂簃簄簅簆簇簈簉簊簋簌簍簎簏簐簑簒簓簔"
    u8"簕簖簗簘簙簚簛簜簝簞簟簠簡簢簣簤簥簦簧簨簩簪簫簬簭簮簯簰簱簲簳簴"
    u8"簵簶簷簸簹簺簻簼簽簾簿籀籁籂籃籄籅籆籇籈籉籊籋籌籍籎籏籐籑籒籓籔"
    u8"籕籖籗籘籙籚籛籜籝籞籟籠籡籢籣籤籥籦籧籨籩籪籫籬籭籮籯籰籱籲米籴"
    u8"籵籶籷籸籹籺类籼籽籾籿粀粁粂粃粄粅粆粇粈粉粊粋粌粍粎粏粐粑粒粓粔"
    u8"粕粖粗粘粙粚粛粜粝粞粟粠粡粢粣粤粥粦粧粨粩粪粫粬粭粮粯粰粱粲粳粴"
    u8"粵粶粷粸粹粺粻粼粽精粿糀糁糂糃糄糅糆糇糈糉糊糋糌糍糎糏糐糑糒糓糔"
    u8"糕糖糗糘糙糚糛糜糝糞糟糠糡糢糣糤糥糦糧糨糩糪糫糬糭糮糯糰糱糲糳糴"
    u8"糵糶糷糸糹糺系糼糽糾糿紀紁紂紃約紅紆紇紈紉紊紋紌納紎紏紐紑紒紓純"
    u8"紕紖紗紘紙級紛紜紝紞紟素紡索紣紤紥紦紧紨紩紪紫紬紭紮累細紱紲紳紴"
    u8"紵紶紷紸紹紺紻紼紽紾紿絀絁終絃組絅絆絇絈絉絊絋経絍絎絏結絑絒絓絔"
    u8"絕絖絗絘絙絚絛絜絝絞絟絠絡絢絣絤絥給絧絨絩絪絫絬絭絮絯絰統絲絳絴"
    u8"絵絶絷絸絹絺絻絼絽絾絿綀綁綂綃綄綅綆綇綈綉綊綋綌綍綎綏綐綑綒經綔"
    u8"綕綖綗綘継続綛綜綝綞綟綠綡綢綣綤綥綦綧綨綩綪綫綬維綮綯綰綱網綳綴"
    u8"綵綶綷綸綹綺綻綼綽綾綿緀緁緂緃緄緅緆緇緈緉緊緋緌緍緎総緐緑緒緓緔"
    u8"緕緖緗緘緙線緛緜緝緞緟締緡緢緣緤緥緦緧編緩緪緫緬緭緮緯緰緱緲緳練"
    u8"緵緶緷緸緹緺緻緼緽緾緿縀縁縂縃縄縅縆縇縈縉縊縋縌縍縎縏縐縑縒縓縔"
    u8"縕縖縗縘縙縚縛縜縝縞縟縠縡縢縣縤縥縦縧縨縩縪縫縬縭縮縯縰縱縲縳縴"
    u8"縵縶縷縸縹縺縻縼總績縿繀繁繂繃繄繅繆繇繈繉繊繋繌繍繎繏繐繑繒繓織"
    u8"繕繖繗繘繙繚繛繜繝繞繟繠繡繢繣繤繥繦繧繨繩繪繫繬繭繮繯繰繱繲繳繴"
    u8"繵繶繷繸繹繺繻繼繽繾繿纀纁纂纃纄纅纆纇纈纉纊纋續纍纎纏纐纑纒纓纔"
    u8"纕纖纗纘纙纚纛纜纝纞纟纠纡红纣纤纥约级纨纩纪纫纬纭纮纯纰纱纲纳纴"
    u8"纵纶纷纸纹纺纻纼纽纾线绀绁绂练组绅细织终绉绊绋绌绍绎经绐绑绒结绔"
    u8"绕绖绗绘给绚绛络绝绞统绠绡绢绣绤绥绦继绨绩绪绫绬续绮绯绰绱绲绳维"
    u8"绵绶绷绸绹绺绻综绽绾绿缀缁缂缃缄缅缆缇缈缉缊缋缌缍缎缏缐缑缒缓缔"
    u8"缕编缗缘缙缚缛缜缝缞缟缠缡缢缣缤缥缦缧缨缩缪缫缬缭缮缯缰缱缲缳缴"
    u8"缵缶缷缸缹缺缻缼缽缾缿罀罁罂罃罄罅罆罇罈罉罊罋罌罍罎罏罐网罒罓罔"
    u8"罕罖罗罘罙罚罛罜罝罞罟罠罡罢罣罤罥罦罧罨罩罪罫罬罭置罯罰罱署罳罴"
    u8"罵罶罷罸罹罺罻罼罽罾罿羀羁羂羃羄羅羆羇羈羉羊羋羌羍美羏羐羑羒羓羔"
    u8"羕羖羗羘羙羚羛羜羝羞羟羠羡羢羣群羥羦羧羨義羪羫羬羭羮羯羰羱羲羳羴"
    u8"羵羶羷羸羹羺羻羼羽羾羿翀翁翂翃翄翅翆翇翈翉翊翋翌翍翎翏翐翑習翓翔"
    u8"翕翖翗翘翙翚翛翜翝翞翟翠翡翢翣翤翥翦翧翨翩翪翫翬翭翮翯翰翱翲翳翴"
    u8"翵翶翷翸翹翺翻翼翽翾翿耀老耂考耄者耆耇耈耉耊耋而耍耎耏耐耑耒耓耔"
    u8"耕耖耗耘耙耚耛耜耝耞耟耠耡耢耣耤耥耦耧耨耩耪耫耬耭耮耯耰耱耲耳耴"
    u8"耵耶耷耸耹耺耻耼耽耾耿聀聁聂聃聄聅聆聇聈聉聊聋职聍聎聏聐聑聒聓联"
    u8"聕聖聗聘聙聚聛聜聝聞聟聠聡聢聣聤聥聦聧聨聩聪聫聬聭聮聯聰聱聲聳聴"
    u8"聵聶職聸聹聺聻聼聽聾聿肀肁肂肃肄肅肆肇肈肉肊肋肌肍肎肏肐肑肒肓肔"
    u8"肕肖肗肘肙肚肛肜肝肞肟肠股肢肣肤肥肦肧肨肩肪肫肬肭肮肯肰肱育肳肴"
    u8"肵肶肷肸肹肺肻肼肽肾肿胀胁胂胃胄胅胆胇胈胉胊胋背胍胎胏胐胑胒胓胔"
    u8"胕胖胗胘胙胚胛胜胝胞胟胠胡胢胣胤胥胦胧胨胩胪胫胬胭胮胯胰胱胲胳胴"
    u8"胵胶胷胸胹胺胻胼能胾胿脀脁脂脃脄脅脆脇脈脉脊脋脌脍脎脏脐脑脒脓脔"
    u8"脕脖脗脘脙脚脛脜脝脞脟脠脡脢脣脤脥脦脧脨脩脪脫脬脭脮脯脰脱脲脳脴"
    u8"脵脶脷脸脹脺脻脼脽脾脿腀腁腂腃腄腅腆腇腈腉腊腋腌腍腎腏腐腑腒腓腔"
    u8"腕腖腗腘腙腚腛腜腝腞腟腠腡腢腣腤腥腦腧腨腩腪腫腬腭腮腯腰腱腲腳腴"
    u8"腵腶腷腸腹腺腻腼腽腾腿膀膁膂膃膄膅膆膇膈膉膊膋膌膍膎膏膐膑膒膓膔"
    u8"膕膖膗膘膙膚膛膜膝膞膟膠膡膢膣膤膥膦膧膨膩膪膫膬膭膮膯膰膱膲膳膴"
    u8"膵膶膷膸膹膺膻膼膽膾膿臀臁臂臃臄臅臆臇臈臉臊臋臌臍臎臏臐臑臒臓臔"
    u8"臕臖臗臘臙臚臛臜臝臞臟臠臡臢臣臤臥臦臧臨臩自臫臬臭臮臯臰臱臲至致"
    u8"臵臶臷臸臹臺臻臼臽臾臿舀舁舂舃舄舅舆與興舉舊舋舌舍舎舏舐舑舒舓舔"
    u8"舕舖舗舘舙舚舛舜舝舞舟舠舡舢舣舤舥舦舧舨舩航舫般舭舮舯舰舱舲舳舴"
    u8"舵舶舷舸船舺舻舼舽舾舿艀艁艂艃艄艅艆艇艈艉艊艋艌艍艎艏艐艑艒艓艔"
    u8"艕艖艗艘艙艚艛艜艝艞艟艠艡艢艣艤艥艦艧艨艩艪艫艬艭艮良艰艱色艳艴"
    u8"艵艶艷艸艹艺艻艼艽艾艿芀芁节芃芄芅芆芇芈芉芊芋芌芍芎芏芐芑芒芓芔"
    u8"芕芖芗芘芙芚芛芜芝芞芟芠芡芢芣芤芥芦芧芨芩芪芫芬芭芮芯芰花芲芳芴"
    u8"芵芶芷芸芹芺芻芼芽芾芿苀苁苂苃苄苅苆苇苈苉苊苋苌苍苎苏苐苑苒苓苔"
    u8"苕苖苗苘苙苚苛苜苝苞苟苠苡苢苣苤若苦苧苨苩苪苫苬苭苮苯苰英苲苳苴"
    u8"苵苶苷苸苹苺苻苼苽苾苿茀茁茂范茄茅茆茇茈茉茊茋茌茍茎茏茐茑茒茓茔"
    u8"茕茖茗茘茙茚茛茜茝茞茟茠茡茢茣茤茥茦茧茨茩茪茫茬茭茮茯茰茱茲茳茴"
    u8"茵茶茷茸茹茺茻茼茽茾茿荀荁荂荃荄荅荆荇荈草荊荋荌荍荎荏荐荑荒荓荔"
    u8"荕荖荗荘荙荚荛荜荝荞荟荠荡荢荣荤荥荦荧荨荩荪荫荬荭荮药荰荱荲荳荴"
    u8"荵荶荷荸荹荺荻荼荽荾荿莀莁莂莃莄莅莆莇莈莉莊莋莌莍莎莏莐莑莒莓莔"
    u8"莕莖莗莘莙莚莛莜莝莞莟莠莡莢莣莤莥莦莧莨莩莪莫莬莭莮莯莰莱莲莳莴"
    u8"莵莶获莸莹莺莻莼莽莾莿菀菁菂菃菄菅菆菇菈菉菊菋菌菍菎菏菐菑菒菓菔"
    u8"菕菖菗菘菙菚菛菜菝菞菟菠菡菢菣菤菥菦菧菨菩菪菫菬菭菮華菰菱菲菳菴"
    u8"菵菶菷菸菹菺菻菼菽菾菿萀萁萂萃萄萅萆萇萈萉萊萋萌萍萎萏萐萑萒萓萔"
    u8"萕萖萗萘萙萚萛萜萝萞萟萠萡萢萣萤营萦萧萨萩萪萫萬萭萮萯萰萱萲萳萴"
    u8"萵萶萷萸萹萺萻萼落萾萿葀葁葂葃葄葅葆葇葈葉葊葋葌葍葎葏葐葑葒葓葔"
    u8"葕葖著葘葙葚葛葜葝葞葟葠葡葢董葤葥葦葧葨葩葪葫葬葭葮葯葰葱葲葳葴"
    u8"葵葶葷葸葹葺葻葼葽葾葿蒀蒁蒂蒃蒄蒅蒆蒇蒈蒉蒊蒋蒌蒍蒎蒏蒐蒑蒒蒓蒔"
    u8"蒕蒖蒗蒘蒙蒚蒛蒜蒝蒞蒟蒠蒡蒢蒣蒤蒥蒦蒧蒨蒩蒪蒫蒬蒭蒮蒯蒰蒱蒲蒳蒴"
    u8"蒵蒶蒷蒸蒹蒺蒻蒼蒽蒾蒿蓀蓁蓂蓃蓄蓅蓆蓇蓈蓉蓊蓋蓌蓍蓎蓏蓐蓑蓒蓓蓔"
    u8"蓕蓖蓗蓘蓙蓚蓛蓜蓝蓞蓟蓠蓡蓢蓣蓤蓥蓦蓧蓨蓩蓪蓫蓬蓭蓮蓯蓰蓱蓲蓳蓴"
    u8"蓵蓶蓷蓸蓹蓺蓻蓼蓽蓾蓿蔀蔁蔂蔃蔄蔅蔆蔇蔈蔉蔊蔋蔌蔍蔎蔏蔐蔑蔒蔓蔔"
    u8"蔕蔖蔗蔘蔙蔚蔛蔜蔝蔞蔟蔠蔡蔢蔣蔤蔥蔦蔧蔨蔩蔪蔫蔬蔭蔮蔯蔰蔱蔲蔳蔴"
    u8"蔵蔶蔷蔸蔹蔺蔻蔼蔽蔾蔿蕀蕁蕂蕃蕄蕅蕆蕇蕈蕉蕊蕋蕌蕍蕎蕏蕐蕑蕒蕓蕔"
    u8"蕕蕖蕗蕘蕙蕚蕛蕜蕝蕞蕟蕠蕡蕢蕣蕤蕥蕦蕧蕨蕩蕪蕫蕬蕭蕮蕯蕰蕱蕲蕳蕴"
    u8"蕵蕶蕷蕸蕹蕺蕻蕼蕽蕾蕿薀薁薂薃薄薅薆薇薈薉薊薋薌薍薎薏薐薑薒薓薔"
    u8"薕薖薗薘薙薚薛薜薝薞薟薠薡薢薣薤薥薦薧薨薩薪薫薬薭薮薯薰薱薲薳薴"
    u8"薵薶薷薸薹薺薻薼薽薾薿藀藁藂藃藄藅藆藇藈藉藊藋藌藍藎藏藐藑藒藓藔"
    u8"藕藖藗藘藙藚藛藜藝藞藟藠藡藢藣藤藥藦藧藨藩藪藫藬藭藮藯藰藱藲藳藴"
    u8"藵藶藷藸藹藺藻藼藽藾藿蘀蘁蘂蘃蘄蘅蘆蘇蘈蘉蘊蘋蘌蘍蘎蘏蘐蘑蘒蘓蘔"
    u8"蘕蘖蘗蘘蘙蘚蘛蘜蘝蘞蘟蘠蘡蘢蘣蘤蘥蘦蘧蘨蘩蘪蘫蘬蘭蘮蘯蘰蘱蘲蘳蘴"
    u8"蘵蘶蘷蘸蘹蘺蘻蘼蘽蘾蘿虀虁虂虃虄虅虆虇虈虉虊虋虌虍虎虏虐虑虒虓虔"
    u8"處虖虗虘虙虚虛虜虝虞號虠虡虢虣虤虥虦虧虨虩虪虫虬虭虮虯虰虱虲虳虴"
    u8"虵虶虷虸虹虺虻虼虽虾虿蚀蚁蚂蚃蚄蚅蚆蚇蚈蚉蚊蚋蚌蚍蚎蚏蚐蚑蚒蚓蚔"
    u8"蚕蚖蚗蚘蚙蚚蚛蚜蚝蚞蚟蚠蚡蚢蚣蚤蚥蚦蚧蚨蚩蚪蚫蚬蚭蚮蚯蚰蚱蚲蚳蚴"
    u8"蚵蚶蚷蚸蚹蚺蚻蚼蚽蚾蚿蛀蛁蛂蛃蛄蛅蛆蛇蛈蛉蛊蛋蛌蛍蛎蛏蛐蛑蛒蛓蛔"
    u8"蛕蛖蛗蛘蛙蛚蛛蛜蛝蛞蛟蛠蛡蛢蛣蛤蛥蛦蛧蛨蛩蛪蛫蛬蛭蛮蛯蛰蛱蛲蛳蛴"
    u8"蛵蛶蛷蛸蛹蛺蛻蛼蛽蛾蛿蜀蜁蜂蜃蜄蜅蜆蜇蜈蜉蜊蜋蜌蜍蜎蜏蜐蜑蜒蜓蜔"
    u8"蜕蜖蜗蜘蜙蜚蜛蜜蜝蜞蜟蜠蜡蜢蜣蜤蜥蜦蜧蜨蜩蜪蜫蜬蜭蜮蜯蜰蜱蜲蜳蜴"
    u8"蜵蜶蜷蜸蜹蜺蜻蜼蜽蜾蜿蝀蝁蝂蝃蝄蝅蝆蝇蝈蝉蝊蝋蝌蝍蝎蝏蝐蝑蝒蝓蝔"
    u8"蝕蝖蝗蝘蝙蝚蝛蝜蝝蝞蝟蝠蝡蝢蝣蝤蝥蝦蝧蝨蝩蝪蝫蝬蝭蝮蝯蝰蝱蝲蝳蝴"
    u8"蝵蝶蝷蝸蝹蝺蝻蝼蝽蝾蝿螀螁螂螃螄螅螆螇螈螉螊螋螌融螎螏螐螑螒螓螔"
    u8"螕螖螗螘螙螚螛螜螝螞螟螠螡螢螣螤螥螦螧螨螩螪螫螬螭螮螯螰螱螲螳螴"
    u8"螵螶螷螸螹螺螻螼螽螾螿蟀蟁蟂蟃蟄蟅蟆蟇蟈蟉蟊蟋蟌蟍蟎蟏蟐蟑蟒蟓蟔"
    u8"蟕蟖蟗蟘蟙蟚蟛蟜蟝蟞蟟蟠蟡蟢蟣蟤蟥蟦蟧蟨蟩蟪蟫蟬蟭蟮蟯蟰蟱蟲蟳蟴"
    u8"蟵蟶蟷蟸蟹蟺蟻蟼蟽蟾蟿蠀蠁蠂蠃蠄蠅蠆蠇蠈蠉蠊蠋蠌蠍蠎蠏蠐蠑蠒蠓蠔"
    u8"蠕蠖蠗蠘蠙蠚蠛蠜蠝蠞蠟蠠蠡蠢蠣蠤蠥蠦蠧蠨蠩蠪蠫蠬蠭蠮蠯蠰蠱蠲蠳蠴"
    u8"蠵蠶蠷蠸蠹蠺蠻蠼蠽蠾蠿血衁衂衃衄衅衆衇衈衉衊衋行衍衎衏衐衑衒術衔"
    u8"衕衖街衘衙衚衛衜衝衞衟衠衡衢衣衤补衦衧表衩衪衫衬衭衮衯衰衱衲衳衴"
    u8"衵衶衷衸衹衺衻衼衽衾衿袀袁袂袃袄袅袆袇袈袉袊袋袌袍袎袏袐袑袒袓袔"
    u8"袕袖袗袘袙袚袛袜袝袞袟袠袡袢袣袤袥袦袧袨袩袪被袬袭袮袯袰袱袲袳袴"
    u8"袵袶袷袸袹袺袻袼袽袾袿裀裁裂裃裄装裆裇裈裉裊裋裌裍裎裏裐裑裒裓裔"
    u8"裕裖裗裘裙裚裛補裝裞裟裠裡裢裣裤裥裦裧裨裩裪裫裬裭裮裯裰裱裲裳裴"
    u8"裵裶裷裸裹裺裻裼製裾裿褀褁褂褃褄褅褆複褈褉褊褋褌褍褎褏褐褑褒褓褔"
    u8"褕褖褗褘褙褚褛褜褝褞褟褠褡褢褣褤褥褦褧褨褩褪褫褬褭褮褯褰褱褲褳褴"
    u8"褵褶褷褸褹褺褻褼褽褾褿襀襁襂襃襄襅襆襇襈襉襊襋襌襍襎襏襐襑襒襓襔"
    u8"襕襖襗襘襙襚襛襜襝襞襟襠襡襢襣襤襥襦襧襨襩襪襫襬襭襮襯襰襱襲襳襴"
    u8"襵襶襷襸襹襺襻襼襽襾西覀要覂覃覄覅覆覇覈覉覊見覌覍覎規覐覑覒覓覔"
    u8"覕視覗覘覙覚覛覜覝覞覟覠覡覢覣覤覥覦覧覨覩親覫覬覭覮覯覰覱覲観覴"
    u8"覵覶覷覸覹覺覻覼覽覾覿觀见观觃规觅视觇览觉觊觋觌觍觎觏觐觑角觓觔"
    u8"觕觖觗觘觙觚觛觜觝觞觟觠觡觢解觤觥触觧觨觩觪觫觬觭觮觯觰觱觲觳觴"
    u8"觵觶觷觸觹觺觻觼觽觾觿言訁訂訃訄訅訆訇計訉訊訋訌訍討訏訐訑訒訓訔"
    u8"訕訖託記訙訚訛訜訝訞訟訠訡訢訣訤訥訦訧訨訩訪訫訬設訮訯訰許訲訳訴"
    u8"訵訶訷訸訹診註証訽訾訿詀詁詂詃詄詅詆詇詈詉詊詋詌詍詎詏詐詑詒詓詔"
    u8"評詖詗詘詙詚詛詜詝詞詟詠詡詢詣詤詥試詧詨詩詪詫詬詭詮詯詰話該詳詴"
    u8"詵詶詷詸詹詺詻詼詽詾詿誀誁誂誃誄誅誆誇誈誉誊誋誌認誎誏誐誑誒誓誔"
    u8"誕誖誗誘誙誚誛誜誝語誟誠誡誢誣誤誥誦誧誨誩說誫説読誮誯誰誱課誳誴"
    u8"誵誶誷誸誹誺誻誼誽誾調諀諁諂諃諄諅諆談諈諉諊請諌諍諎諏諐諑諒諓諔"
    u8"諕論諗諘諙諚諛諜諝諞諟諠諡諢諣諤諥諦諧諨諩諪諫諬諭諮諯諰諱諲諳諴"
    u8"諵諶諷諸諹諺諻諼諽諾諿謀謁謂謃謄謅謆謇謈謉謊謋謌謍謎謏謐謑謒謓謔"
    u8"謕謖謗謘謙謚講謜謝謞謟謠謡謢謣謤謥謦謧謨謩謪謫謬謭謮謯謰謱謲謳謴"
    u8"謵謶謷謸謹謺謻謼謽謾謿譀譁譂譃譄譅譆譇譈證譊譋譌譍譎譏譐譑譒譓譔"
    u8"譕譖譗識譙譚譛譜譝譞譟譠譡譢譣譤譥警譧譨譩譪譫譬譭譮譯議譱譲譳譴"
    u8"譵譶護譸譹譺譻譼譽譾譿讀讁讂讃讄讅讆讇讈讉變讋讌讍讎讏讐讑讒讓讔"
    u8"讕讖讗讘讙讚讛讜讝讞讟讠计订讣认讥讦讧讨让讪讫讬训议讯记讱讲讳讴"
    u8"讵讶讷许讹论讻讼讽设访诀证诂诃评诅识诇诈诉诊诋诌词诎诏诐译诒诓诔"
    u8"试诖诗诘诙诚诛诜话诞诟诠诡询诣诤该详诧诨诩诪诫诬语诮误诰诱诲诳说"
    u8"诵诶请诸诹诺读诼诽课诿谀谁谂调谄谅谆谇谈谉谊谋谌谍谎谏谐谑谒谓谔"
    u8"谕谖谗谘谙谚谛谜谝谞谟谠谡谢谣谤谥谦谧谨谩谪谫谬谭谮谯谰谱谲谳谴"
    u8"谵谶谷谸谹谺谻谼谽谾谿豀豁豂豃豄豅豆豇豈豉豊豋豌豍豎豏豐豑豒豓豔"
    u8"豕豖豗豘豙豚豛豜豝豞豟豠象豢豣豤豥豦豧豨豩豪豫豬豭豮豯豰豱豲豳豴"
    u8"豵豶豷豸豹豺豻豼豽豾豿貀貁貂貃貄貅貆貇貈貉貊貋貌貍貎貏貐貑貒貓貔"
    u8"貕貖貗貘貙貚貛貜貝貞貟負財貢貣貤貥貦貧貨販貪貫責貭貮貯貰貱貲貳貴"
    u8"貵貶買貸貹貺費貼貽貾貿賀賁賂賃賄賅賆資賈賉賊賋賌賍賎賏賐賑賒賓賔"
    u8"賕賖賗賘賙賚賛賜賝賞賟賠賡賢賣賤賥賦賧賨賩質賫賬賭賮賯賰賱賲賳賴"
    u8"賵賶賷賸賹賺賻購賽賾賿贀贁贂贃贄贅贆贇贈贉贊贋贌贍贎贏贐贑贒贓贔"
    u8"贕贖贗贘贙贚贛贜贝贞负贠贡财责贤败账货质贩贪贫贬购贮贯贰贱贲贳贴"
    u8"贵贶贷贸费贺贻贼贽贾贿赀赁赂赃资赅赆赇赈赉赊赋赌赍赎赏赐赑赒赓赔"
    u8"赕赖赗赘赙赚赛赜赝赞赟赠赡赢赣赤赥赦赧赨赩赪赫赬赭赮赯走赱赲赳赴"
    u8"赵赶起赸赹赺赻赼赽赾赿趀趁趂趃趄超趆趇趈趉越趋趌趍趎趏趐趑趒趓趔"
    u8"趕趖趗趘趙趚趛趜趝趞趟趠趡趢趣趤趥趦趧趨趩趪趫趬趭趮趯趰趱趲足趴"
    u8"趵趶趷趸趹趺趻趼趽趾趿跀跁跂跃跄跅跆跇跈跉跊跋跌跍跎跏跐跑跒跓跔"
    u8"跕跖跗跘跙跚跛跜距跞跟跠跡跢跣跤跥跦跧跨跩跪跫跬跭跮路跰跱跲跳跴"
    u8"践跶跷跸跹跺跻跼跽跾跿踀踁踂踃踄踅踆踇踈踉踊踋踌踍踎踏踐踑踒踓踔"
    u8"踕踖踗踘踙踚踛踜踝踞踟踠踡踢踣踤踥踦踧踨踩踪踫踬踭踮踯踰踱踲踳踴"
    u8"踵踶踷踸踹踺踻踼踽踾踿蹀蹁蹂蹃蹄蹅蹆蹇蹈蹉蹊蹋蹌蹍蹎蹏蹐蹑蹒蹓蹔"
    u8"蹕蹖蹗蹘蹙蹚蹛蹜蹝蹞蹟蹠蹡蹢蹣蹤蹥蹦蹧蹨蹩蹪蹫蹬蹭蹮蹯蹰蹱蹲蹳蹴"
    u8"蹵蹶蹷蹸蹹蹺蹻蹼蹽蹾蹿躀躁躂躃躄躅躆躇躈躉躊躋躌躍躎躏躐躑躒躓躔"
    u8"躕躖躗躘躙躚躛躜躝躞躟躠躡躢躣躤躥躦躧躨躩躪身躬躭躮躯躰躱躲躳躴"
    u8"躵躶躷躸躹躺躻躼躽躾躿軀軁軂軃軄軅軆軇軈軉車軋軌軍軎軏軐軑軒軓軔"
    u8"軕軖軗軘軙軚軛軜軝軞軟軠軡転軣軤軥軦軧軨軩軪軫軬軭軮軯軰軱軲軳軴"
    u8"軵軶軷軸軹軺軻軼軽軾軿輀輁輂較輄輅輆輇輈載輊輋輌輍輎輏輐輑輒輓輔"
    u8"輕輖輗輘輙輚輛輜輝輞輟輠輡輢輣輤輥輦輧輨輩輪輫輬輭輮輯輰輱輲輳輴"
    u8"輵輶輷輸輹輺輻輼輽輾輿轀轁轂轃轄轅轆轇轈轉轊轋轌轍轎轏轐轑轒轓轔"
    u8"轕轖轗轘轙轚轛轜轝轞轟轠轡轢轣轤轥车轧轨轩轪轫转轭轮软轰轱轲轳轴"
    u8"轵轶轷轸轹轺轻轼载轾轿辀辁辂较辄辅辆辇辈辉辊辋辌辍辎辏辐辑辒输辔"
    u8"辕辖辗辘辙辚辛辜辝辞辟辠辡辢辣辤辥辦辧辨辩辪辫辬辭辮辯辰辱農辳辴"
    u8"辵辶辷辸边辺辻込辽达辿迀迁迂迃迄迅迆过迈迉迊迋迌迍迎迏运近迒迓返"
    u8"迕迖迗还这迚进远违连迟迠迡迢迣迤迥迦迧迨迩迪迫迬迭迮迯述迱迲迳迴"
    u8"迵迶迷迸迹迺迻迼追迾迿退送适逃逄逅逆逇逈选逊逋逌逍逎透逐逑递逓途"
    u8"逕逖逗逘這通逛逜逝逞速造逡逢連逤逥逦逧逨逩逪逫逬逭逮逯逰週進逳逴"
    u8"逵逶逷逸逹逺逻逼逽逾逿遀遁遂遃遄遅遆遇遈遉遊運遌遍過遏遐遑遒道達"
    u8"違遖遗遘遙遚遛遜遝遞遟遠遡遢遣遤遥遦遧遨適遪遫遬遭遮遯遰遱遲遳遴"
    u8"遵遶遷選遹遺遻遼遽遾避邀邁邂邃還邅邆邇邈邉邊邋邌邍邎邏邐邑邒邓邔"
    u8"邕邖邗邘邙邚邛邜邝邞邟邠邡邢那邤邥邦邧邨邩邪邫邬邭邮邯邰邱邲邳邴"
    u8"邵邶邷邸邹邺邻邼邽邾邿郀郁郂郃郄郅郆郇郈郉郊郋郌郍郎郏郐郑郒郓郔"
    u8"郕郖郗郘郙郚郛郜郝郞郟郠郡郢郣郤郥郦郧部郩郪郫郬郭郮郯郰郱郲郳郴"
    u8"郵郶郷郸郹郺郻郼都郾郿鄀鄁鄂鄃鄄鄅鄆鄇鄈鄉鄊鄋鄌鄍鄎鄏鄐鄑鄒鄓鄔"
    u8"鄕鄖鄗鄘鄙鄚鄛鄜鄝鄞鄟鄠鄡鄢鄣鄤鄥鄦鄧鄨鄩鄪鄫鄬鄭鄮鄯鄰鄱鄲鄳鄴"
    u8"鄵鄶鄷鄸鄹鄺鄻鄼鄽鄾鄿酀酁酂酃酄酅酆酇酈酉酊酋酌配酎酏酐酑酒酓酔"
    u8"酕酖酗酘酙酚酛酜酝酞酟酠酡酢酣酤酥酦酧酨酩酪酫酬酭酮酯酰酱酲酳酴"
    u8"酵酶酷酸酹酺酻酼酽酾酿醀醁醂醃醄醅醆醇醈醉醊醋醌醍醎醏醐醑醒醓醔"
    u8"醕醖醗醘醙醚醛醜醝醞醟醠醡醢醣醤醥醦醧醨醩醪醫醬醭醮醯醰醱醲醳醴"
    u8"醵醶醷醸醹醺醻醼醽醾醿釀釁釂釃釄釅釆采釈釉释釋里重野量釐金釒釓釔"
    u8"釕釖釗釘釙釚釛釜針釞釟釠釡釢釣釤釥釦釧釨釩釪釫釬釭釮釯釰釱釲釳釴"
    u8"釵釶釷釸釹釺釻釼釽釾釿鈀鈁鈂鈃鈄鈅鈆鈇鈈鈉鈊鈋鈌鈍鈎鈏鈐鈑鈒鈓鈔"
    u8"鈕鈖鈗鈘鈙鈚鈛鈜鈝鈞鈟鈠鈡鈢鈣鈤鈥鈦鈧鈨鈩鈪鈫鈬鈭鈮鈯鈰鈱鈲鈳鈴"
    u8"鈵鈶鈷鈸鈹鈺鈻鈼鈽鈾鈿鉀鉁鉂鉃鉄鉅鉆鉇鉈鉉鉊鉋鉌鉍鉎鉏鉐鉑鉒鉓鉔"
    u8"鉕鉖鉗鉘鉙鉚鉛鉜鉝鉞鉟鉠鉡鉢鉣鉤鉥鉦鉧鉨鉩鉪鉫鉬鉭鉮鉯鉰鉱鉲鉳鉴"
    u8"鉵鉶鉷鉸鉹鉺鉻鉼鉽鉾鉿銀銁銂銃銄銅銆銇銈銉銊銋銌銍銎銏銐銑銒銓銔"
    u8"銕銖銗銘銙銚銛銜銝銞銟銠銡銢銣銤銥銦銧銨銩銪銫銬銭銮銯銰銱銲銳銴"
    u8"銵銶銷銸銹銺銻銼銽銾銿鋀鋁鋂鋃鋄鋅鋆鋇鋈鋉鋊鋋鋌鋍鋎鋏鋐鋑鋒鋓鋔"
    u8"鋕鋖鋗鋘鋙鋚鋛鋜鋝鋞鋟鋠鋡鋢鋣鋤鋥鋦鋧鋨鋩鋪鋫鋬鋭鋮鋯鋰鋱鋲鋳鋴"
    u8"鋵鋶鋷鋸鋹鋺鋻鋼鋽鋾鋿錀錁錂錃錄錅錆錇錈錉錊錋錌錍錎錏錐錑錒錓錔"
    u8"錕錖錗錘錙錚錛錜錝錞錟錠錡錢錣錤錥錦錧錨錩錪錫錬錭錮錯錰錱録錳錴"
    u8"錵錶錷錸錹錺錻錼錽錾錿鍀鍁鍂鍃鍄鍅鍆鍇鍈鍉鍊鍋鍌鍍鍎鍏鍐鍑鍒鍓鍔"
    u8"鍕鍖鍗鍘鍙鍚鍛鍜鍝鍞鍟鍠鍡鍢鍣鍤鍥鍦鍧鍨鍩鍪鍫鍬鍭鍮鍯鍰鍱鍲鍳鍴"
    u8"鍵鍶鍷鍸鍹鍺鍻鍼鍽鍾鍿鎀鎁鎂鎃鎄鎅鎆鎇鎈鎉鎊鎋鎌鎍鎎鎏鎐鎑鎒鎓鎔"
    u8"鎕鎖鎗鎘鎙鎚鎛鎜鎝鎞鎟鎠鎡鎢鎣鎤鎥鎦鎧鎨鎩鎪鎫鎬鎭鎮鎯鎰鎱鎲鎳鎴"
    u8"鎵鎶鎷鎸鎹鎺鎻鎼鎽鎾鎿鏀鏁鏂鏃鏄鏅鏆鏇鏈鏉鏊鏋鏌鏍鏎鏏鏐鏑鏒鏓鏔"
    u8"鏕鏖鏗鏘鏙鏚鏛鏜鏝鏞鏟鏠鏡鏢鏣鏤鏥鏦鏧鏨鏩鏪鏫鏬鏭鏮鏯鏰鏱鏲鏳鏴"
    u8"鏵鏶鏷鏸鏹鏺鏻鏼鏽鏾鏿鐀鐁鐂鐃鐄鐅鐆鐇鐈鐉鐊鐋鐌鐍鐎鐏鐐鐑鐒鐓鐔"
    u8"鐕鐖鐗鐘鐙鐚鐛鐜鐝鐞鐟鐠鐡鐢鐣鐤鐥鐦鐧鐨鐩鐪鐫鐬鐭鐮鐯鐰鐱鐲鐳鐴"
    u8"鐵鐶鐷鐸鐹鐺鐻鐼鐽鐾鐿鑀鑁鑂鑃鑄鑅鑆鑇鑈鑉鑊鑋鑌鑍鑎鑏鑐鑑鑒鑓鑔"
    u8"鑕鑖鑗鑘鑙鑚鑛鑜鑝鑞鑟鑠鑡鑢鑣鑤鑥鑦鑧鑨鑩鑪鑫鑬鑭鑮鑯鑰鑱鑲鑳鑴"
    u8"鑵鑶鑷鑸鑹鑺鑻鑼鑽鑾鑿钀钁钂钃钄钅钆钇针钉钊钋钌钍钎钏钐钑钒钓钔"
    u8"钕钖钗钘钙钚钛钜钝钞钟钠钡钢钣钤钥钦钧钨钩钪钫钬钭钮钯钰钱钲钳钴"
    u8"钵钶钷钸钹钺钻钼钽钾钿铀铁铂铃铄铅铆铇铈铉铊铋铌铍铎铏铐铑铒铓铔"
    u8"铕铖铗铘铙铚铛铜铝铞铟铠铡铢铣铤铥铦铧铨铩铪铫铬铭铮铯铰铱铲铳铴"
    u8"铵银铷铸铹铺铻铼铽链铿销锁锂锃锄锅锆锇锈锉锊锋锌锍锎锏锐锑锒锓锔"
    u8"锕锖锗锘错锚锛锜锝锞锟锠锡锢锣锤锥锦锧锨锩锪锫锬锭键锯锰锱锲锳锴"
    u8"锵锶锷锸锹锺锻锼锽锾锿镀镁镂镃镄镅镆镇镈镉镊镋镌镍镎镏镐镑镒镓镔"
    u8"镕镖镗镘镙镚镛镜镝镞镟镠镡镢镣镤镥镦镧镨镩镪镫镬镭镮镯镰镱镲镳镴"
    u8"镵镶長镸镹镺镻镼镽镾长門閁閂閃閄閅閆閇閈閉閊開閌閍閎閏閐閑閒間閔"
    u8"閕閖閗閘閙閚閛閜閝閞閟閠閡関閣閤閥閦閧閨閩閪閫閬閭閮閯閰閱閲閳閴"
    u8"閵閶閷閸閹閺閻閼閽閾閿闀闁闂闃闄闅闆闇闈闉闊闋闌闍闎闏闐闑闒闓闔"
    u8"闕闖闗闘闙闚闛關闝闞闟闠闡闢闣闤闥闦闧门闩闪闫闬闭问闯闰闱闲闳间"
    u8"闵闶闷闸闹闺闻闼闽闾闿阀阁阂阃阄阅阆阇阈阉阊阋阌阍阎阏阐阑阒阓阔"
    u8"阕阖阗阘阙阚阛阜阝阞队阠阡阢阣阤阥阦阧阨阩阪阫阬阭阮阯阰阱防阳阴"
    u8"阵阶阷阸阹阺阻阼阽阾阿陀陁陂陃附际陆陇陈陉陊陋陌降陎陏限陑陒陓陔"
    u8"陕陖陗陘陙陚陛陜陝陞陟陠陡院陣除陥陦陧陨险陪陫陬陭陮陯陰陱陲陳陴"
    u8"陵陶陷陸陹険陻陼陽陾陿隀隁隂隃隄隅隆隇隈隉隊隋隌隍階随隐隑隒隓隔"
    u8"隕隖隗隘隙隚際障隝隞隟隠隡隢隣隤隥隦隧隨隩險隫隬隭隮隯隰隱隲隳隴"
    u8"隵隶隷隸隹隺隻隼隽难隿雀雁雂雃雄雅集雇雈雉雊雋雌雍雎雏雐雑雒雓雔"
    u8"雕雖雗雘雙雚雛雜雝雞雟雠雡離難雤雥雦雧雨雩雪雫雬雭雮雯雰雱雲雳雴"
    u8"雵零雷雸雹雺電雼雽雾雿需霁霂霃霄霅霆震霈霉霊霋霌霍霎霏霐霑霒霓霔"
    u8"霕霖霗霘霙霚霛霜霝霞霟霠霡霢霣霤霥霦霧霨霩霪霫霬霭霮霯霰霱露霳霴"
    u8"霵霶霷霸霹霺霻霼霽霾霿靀靁靂靃靄靅靆靇靈靉靊靋靌靍靎靏靐靑青靓靔"
    u8"靕靖靗靘静靚靛靜靝非靟靠靡面靣靤靥靦靧靨革靪靫靬靭靮靯靰靱靲靳靴"
    u8"靵靶靷靸靹靺靻靼靽靾靿鞀鞁鞂鞃鞄鞅鞆鞇鞈鞉鞊鞋鞌鞍鞎鞏鞐鞑鞒鞓鞔"
    u8"鞕鞖鞗鞘鞙鞚鞛鞜鞝鞞鞟鞠鞡鞢鞣鞤鞥鞦鞧鞨鞩鞪鞫鞬鞭鞮鞯鞰鞱鞲鞳鞴"
    u8"鞵鞶鞷鞸鞹鞺鞻鞼鞽鞾鞿韀韁韂韃韄韅韆韇韈韉韊韋韌韍韎韏韐韑韒韓韔"
    u8"韕韖韗韘韙韚韛韜韝韞韟韠韡韢韣韤韥韦韧韨韩韪韫韬韭韮韯韰韱韲音韴"
    u8"韵韶韷韸韹韺韻韼韽韾響頀頁頂頃頄項順頇須頉頊頋頌頍頎頏預頑頒頓頔"
    u8"頕頖頗領頙頚頛頜頝頞頟頠頡頢頣頤頥頦頧頨頩頪頫頬頭頮頯頰頱頲頳頴"
    u8"頵頶頷頸頹頺頻頼頽頾頿顀顁顂顃顄顅顆顇顈顉顊顋題額顎顏顐顑顒顓顔"
    u8"顕顖顗願顙顚顛顜顝類顟顠顡顢顣顤顥顦顧顨顩顪顫顬顭顮顯顰顱顲顳顴"
    u8"页顶顷顸项顺须顼顽顾顿颀颁颂颃预颅领颇颈颉颊颋颌颍颎颏颐频颒颓颔"
    u8"颕颖颗题颙颚颛颜额颞颟颠颡颢颣颤颥颦颧風颩颪颫颬颭颮颯颰颱颲颳颴"
    u8"颵颶颷颸颹颺颻颼颽颾颿飀飁飂飃飄飅飆飇飈飉飊飋飌飍风飏飐飑飒飓飔"
    u8"飕飖飗飘飙飚飛飜飝飞食飠飡飢飣飤飥飦飧飨飩飪飫飬飭飮飯飰飱飲飳飴"
    u8"飵飶飷飸飹飺飻飼飽飾飿餀餁餂餃餄餅餆餇餈餉養餋餌餍餎餏餐餑餒餓餔"
    u8"餕餖餗餘餙餚餛餜餝餞餟餠餡餢餣餤餥餦餧館餩餪餫餬餭餮餯餰餱餲餳餴"
    u8"餵餶餷餸餹餺餻餼餽餾餿饀饁饂饃饄饅饆饇饈饉饊饋饌饍饎饏饐饑饒饓饔"
    u8"饕饖饗饘饙饚饛饜饝饞饟饠饡饢饣饤饥饦饧饨饩饪饫饬饭饮饯饰饱饲饳饴"
    u8"饵饶饷饸饹饺饻饼饽饾饿馀馁馂馃馄馅馆馇馈馉馊馋馌馍馎馏馐馑馒馓馔"
    u8"馕首馗馘香馚馛馜馝馞馟馠馡馢馣馤馥馦馧馨馩馪馫馬馭馮馯馰馱馲馳馴"
    u8"馵馶馷馸馹馺馻馼馽馾馿駀駁駂駃駄駅駆駇駈駉駊駋駌駍駎駏駐駑駒駓駔"
    u8"駕駖駗駘駙駚駛駜駝駞駟駠駡駢駣駤駥駦駧駨駩駪駫駬駭駮駯駰駱駲駳駴"
    u8"駵駶駷駸駹駺駻駼駽駾駿騀騁騂騃騄騅騆騇騈騉騊騋騌騍騎騏騐騑騒験騔"
    u8"騕騖騗騘騙騚騛騜騝騞騟騠騡騢騣騤騥騦騧騨騩騪騫騬騭騮騯騰騱騲騳騴"
    u8"騵騶騷騸騹騺騻騼騽騾騿驀驁驂驃驄驅驆驇驈驉驊驋驌驍驎驏驐驑驒驓驔"
    u8"驕驖驗驘驙驚驛驜驝驞驟驠驡驢驣驤驥驦驧驨驩驪驫马驭驮驯驰驱驲驳驴"
    u8"驵驶驷驸驹驺驻驼驽驾驿骀骁骂骃骄骅骆骇骈骉骊骋验骍骎骏骐骑骒骓骔"
    u8"骕骖骗骘骙骚骛骜骝骞骟骠骡骢骣骤骥骦骧骨骩骪骫骬骭骮骯骰骱骲骳骴"
    u8"骵骶骷骸骹骺骻骼骽骾骿髀髁髂髃髄髅髆髇髈髉髊髋髌髍髎髏髐髑髒髓體"
    u8"髕髖髗高髙髚髛髜髝髞髟髠髡髢髣髤髥髦髧髨髩髪髫髬髭髮髯髰髱髲髳髴"
    u8"髵髶髷髸髹髺髻髼髽髾髿鬀鬁鬂鬃鬄鬅鬆鬇鬈鬉鬊鬋鬌鬍鬎鬏鬐鬑鬒鬓鬔"
    u8"鬕鬖鬗鬘鬙鬚鬛鬜鬝鬞鬟鬠鬡鬢鬣鬤鬥鬦鬧鬨鬩鬪鬫鬬鬭鬮鬯鬰鬱鬲鬳鬴"
    u8"鬵鬶鬷鬸鬹鬺鬻鬼鬽鬾鬿魀魁魂魃魄魅魆魇魈魉魊魋魌魍魎魏魐魑魒魓魔"
    u8"魕魖魗魘魙魚魛魜魝魞魟魠魡魢魣魤魥魦魧魨魩魪魫魬魭魮魯魰魱魲魳魴"
    u8"魵魶魷魸魹魺魻魼魽魾魿鮀鮁鮂鮃鮄鮅鮆鮇鮈鮉鮊鮋鮌鮍鮎鮏鮐鮑鮒鮓鮔"
    u8"鮕鮖鮗鮘鮙鮚鮛鮜鮝鮞鮟鮠鮡鮢鮣鮤鮥鮦鮧鮨鮩鮪鮫鮬鮭鮮鮯鮰鮱鮲鮳鮴"
    u8"鮵鮶鮷鮸鮹鮺鮻鮼鮽鮾鮿鯀鯁鯂鯃鯄鯅鯆鯇鯈鯉鯊鯋鯌鯍鯎鯏鯐鯑鯒鯓鯔"
    u8"鯕鯖鯗鯘鯙鯚鯛鯜鯝鯞鯟鯠鯡鯢鯣鯤鯥鯦鯧鯨鯩鯪鯫鯬鯭鯮鯯鯰鯱鯲鯳鯴"
    u8"鯵鯶鯷鯸鯹鯺鯻鯼鯽鯾鯿鰀鰁鰂鰃鰄鰅鰆鰇鰈鰉鰊鰋鰌鰍鰎鰏鰐鰑鰒鰓鰔"
    u8"鰕鰖鰗鰘鰙鰚鰛鰜鰝鰞鰟鰠鰡鰢鰣鰤鰥鰦鰧鰨鰩鰪鰫鰬鰭鰮鰯鰰鰱鰲鰳鰴"
    u8"鰵鰶鰷鰸鰹鰺鰻鰼鰽鰾鰿鱀鱁鱂鱃鱄鱅鱆鱇鱈鱉鱊鱋鱌鱍鱎鱏鱐鱑鱒鱓鱔"
    u8"鱕鱖鱗鱘鱙鱚鱛鱜鱝鱞鱟鱠鱡鱢鱣鱤鱥鱦鱧鱨鱩鱪鱫鱬鱭鱮鱯鱰鱱鱲鱳鱴"
    u8"鱵鱶鱷鱸鱹鱺鱻鱼鱽鱾鱿鲀鲁鲂鲃鲄鲅鲆鲇鲈鲉鲊鲋鲌鲍鲎鲏鲐鲑鲒鲓鲔"
    u8"鲕鲖鲗鲘鲙鲚鲛鲜鲝鲞鲟鲠鲡鲢鲣鲤鲥鲦鲧鲨鲩鲪鲫鲬鲭鲮鲯鲰鲱鲲鲳鲴"
    u8"鲵鲶鲷鲸鲹鲺鲻鲼鲽鲾鲿鳀鳁鳂鳃鳄鳅鳆鳇鳈鳉鳊鳋鳌鳍鳎鳏鳐鳑鳒鳓鳔"
    u8"鳕鳖鳗鳘鳙鳚鳛鳜鳝鳞鳟鳠鳡鳢鳣鳤鳥鳦鳧鳨鳩鳪鳫鳬鳭鳮鳯鳰鳱鳲鳳鳴"
    u8"鳵鳶鳷鳸鳹鳺鳻鳼鳽鳾鳿鴀鴁鴂鴃鴄鴅鴆鴇鴈鴉鴊鴋鴌鴍鴎鴏鴐鴑鴒鴓鴔"
    u8"鴕鴖鴗鴘鴙鴚鴛鴜鴝鴞鴟鴠鴡鴢鴣鴤鴥鴦鴧鴨鴩鴪鴫鴬鴭鴮鴯鴰鴱鴲鴳鴴"
    u8"鴵鴶鴷鴸鴹鴺鴻鴼鴽鴾鴿鵀鵁鵂鵃鵄鵅鵆鵇鵈鵉鵊鵋鵌鵍鵎鵏鵐鵑鵒鵓鵔"
    u8"鵕鵖鵗鵘鵙鵚鵛鵜鵝鵞鵟鵠鵡鵢鵣鵤鵥鵦鵧鵨鵩鵪鵫鵬鵭鵮鵯鵰鵱鵲鵳鵴"
    u8"鵵鵶鵷鵸鵹鵺鵻鵼鵽鵾鵿鶀鶁鶂鶃鶄鶅鶆鶇鶈鶉鶊鶋鶌鶍鶎鶏鶐鶑鶒鶓鶔"
    u8"鶕鶖鶗鶘鶙鶚鶛鶜鶝鶞鶟鶠鶡鶢鶣鶤鶥鶦鶧鶨鶩鶪鶫鶬鶭鶮鶯鶰鶱鶲鶳鶴"
    u8"鶵鶶鶷鶸鶹鶺鶻鶼鶽鶾鶿鷀鷁鷂鷃鷄鷅鷆鷇鷈鷉鷊鷋鷌鷍鷎鷏鷐鷑鷒鷓鷔"
    u8"鷕鷖鷗鷘鷙鷚鷛鷜鷝鷞鷟鷠鷡鷢鷣鷤鷥鷦鷧鷨鷩鷪鷫鷬鷭鷮鷯鷰鷱鷲鷳鷴"
    u8"鷵鷶鷷鷸鷹鷺鷻鷼鷽鷾鷿鸀鸁鸂鸃鸄鸅鸆鸇鸈鸉鸊鸋鸌鸍鸎鸏鸐鸑鸒鸓鸔"
    u8"鸕鸖鸗鸘鸙鸚鸛鸜鸝鸞鸟鸠鸡鸢鸣鸤鸥鸦鸧鸨鸩鸪鸫鸬鸭鸮鸯鸰鸱鸲鸳鸴"
    u8"鸵鸶鸷鸸鸹鸺鸻鸼鸽鸾鸿鹀鹁鹂鹃鹄鹅鹆鹇鹈鹉鹊鹋鹌鹍鹎鹏鹐鹑鹒鹓鹔"
    u8"鹕鹖鹗鹘鹙鹚鹛鹜鹝鹞鹟鹠鹡鹢鹣鹤鹥鹦鹧鹨鹩鹪鹫鹬鹭鹮鹯鹰鹱鹲鹳鹴"
    u8"鹵鹶鹷鹸鹹鹺鹻鹼鹽鹾鹿麀麁麂麃麄麅麆麇麈麉麊麋麌麍麎麏麐麑麒麓麔"
    u8"麕麖麗麘麙麚麛麜麝麞麟麠麡麢麣麤麥麦麧麨麩麪麫麬麭麮麯麰麱麲麳麴"
    u8"麵麶麷麸麹麺麻麼麽麾麿黀黁黂黃黄黅黆黇黈黉黊黋黌黍黎黏黐黑黒黓黔"
    u8"黕黖黗默黙黚黛黜黝點黟豈更車賈滑串句龜龜契金喇奈懶癩羅蘿螺裸邏樂"
    u8"洛烙珞落酪駱亂卵欄爛蘭鸞嵐濫藍襤拉臘蠟廊朗浪狼郎來冷勞擄櫓爐盧老"
    u8"蘆虜路露魯鷺碌祿綠菉錄鹿論壟弄籠聾牢磊賂雷壘屢樓淚漏累縷陋勒肋凜"
    u8"凌稜綾菱陵讀拏樂諾丹寧怒率異北磻便復不泌數索參塞省葉說殺辰沈拾若"
    u8"掠略亮兩凉梁糧良諒量勵呂女廬旅濾礪閭驪麗黎力曆歷轢年憐戀撚漣煉璉"
    u8"秊練聯輦蓮連鍊列劣咽烈裂說廉念捻殮簾獵令囹寧嶺怜玲瑩羚聆鈴零靈領"
    u8"例禮醴隸惡了僚寮尿料樂燎療蓼遼龍暈阮劉杻柳流溜琉留硫紐類六戮陸倫"
    u8"崙淪輪律慄栗率隆利吏履易李梨泥理痢罹裏裡里離匿溺吝燐璘藺隣鱗麟林"
    u8"淋臨立笠粒狀炙識什茶刺切度拓糖宅洞暴輻行降見廓兀嗀﨎﨏塚﨑晴﨓﨔"
    u8"凞猪益礼神祥福靖精羽﨟蘒﨡諸﨣﨤逸都﨧﨨﨩飯飼館鶴郞隷侮僧免勉勤"
    u8"卑喝嘆器塀墨層屮悔慨憎懲敏既暑梅海渚漢煮爫琢碑社祉祈祐祖祝禍禎穀"
    u8"突節練縉繁署者臭艹艹著褐視謁謹賓贈辶逸難響頻恵𤋮舘並况全侀充冀勇"
    u8"勺喝啕喙嗢塚墳奄奔婢嬨廒廙彩徭惘慎愈憎慠懲戴揄搜摒敖晴朗望杖歹殺"
    u8"流滛滋漢瀞煮瞧爵犯猪瑱甆画瘝瘟益盛直睊着磌窱節类絛練缾者荒華蝹襁"
    u8"覆視調諸請謁諾諭謹變贈輸遲醙鉶陼難靖韛響頋頻鬒龜𢡊𢡄𣏕㮝䀘䀹𥉉𥳐"
    u8"𧻓齃龎䷀䷁䷂䷃䷄䷅䷆䷇䷈䷉䷊䷋䷌䷍䷎䷏䷐䷑䷒䷓䷔䷕䷖䷗䷘䷙䷚䷛䷜"
    u8"䷝䷞䷟䷠䷡䷢䷣䷤䷥䷦䷧䷨䷩䷪䷫䷬䷭䷮䷯䷰䷱䷲䷳䷴䷵䷶䷷䷸䷹䷺䷻䷼"
    u8"䷽䷾䷿꒐꒑꒒꒓꒔꒕꒖꒗꒘꒙꒚꒛꒜꒝꒞꒟꒠꒡꒢꒣꒤꒥꒦꒧꒨꒩꒪꒫꒬"
    u8"꒭꒮꒯꒰꒱꒲꒳꒴꒵꒶꒷꒸꒹꒺꒻꒼꒽꒾꒿꓀꓁꓂꓃꓄꓅꓆黠黡黢黣黤黥"
    u8"黦黧黨黩黪黫黬黭黮黯ꀀꀁꀂꀃꀄꀅꀆꀇꀈꀉꀊꀋꀌꀍꀎꀏꀐꀑꀒꀓꀔꀕ"
    u8"ꀖꀗꀘꀙꀚꀛꀜꀝꀞꀟꀠꀡꀢꀣꀤꀥꀦꀧꀨꀩꀪꀫꀬꀭꀮꀯꀰꀱꀲꀳꀴꀵ"
    u8"ꀶꀷꀸꀹꀺꀻꀼꀽꀾꀿꁀꁁꁂꁃꁄꁅꁆꁇꁈꁉꁊꁋꁌꁍꁎꁏꁐꁑꁒꁓꁔꁕ"
    u8"ꁖꁗꁘꁙꁚꁛꁜꁝꁞꁟꁠꁡꁢꁣꁤꁥꁦꁧꁨꁩꁪꁫꁬꁭꁮꁯꁰꁱꁲꁳꁴꁵ"
    u8"ꁶꁷꁸꁹꁺꁻꁼꁽꁾꁿꂀꂁꂂꂃꂄꂅꂆꂇꂈꂉꂊꂋꂌꂍꂎꂏꂐꂑꂒꂓꂔꂕ"
    u8"ꂖꂗꂘꂙꂚꂛꂜꂝꂞꂟꂠꂡꂢꂣꂤꂥꂦꂧꂨꂩꂪꂫꂬꂭꂮꂯꂰꂱꂲꂳꂴꂵ"
    u8"ꂶꂷꂸꂹꂺꂻꂼꂽꂾꂿꃀꃁꃂꃃꃄꃅꃆꃇꃈꃉꃊꃋꃌꃍꃎꃏꃐꃑꃒꃓꃔꃕ"
    u8"ꃖꃗꃘꃙꃚꃛꃜꃝꃞꃟꃠꃡꃢꃣꃤꃥꃦꃧꃨꃩꃪꃫꃬꃭꃮꃯꃰꃱꃲꃳꃴꃵ"
    u8"ꃶꃷꃸꃹꃺꃻꃼꃽꃾꃿꄀꄁꄂꄃꄄꄅꄆꄇꄈꄉꄊꄋꄌꄍꄎꄏꄐꄑꄒꄓꄔꄕ"
    u8"ꄖꄗꄘꄙꄚꄛꄜꄝꄞꄟꄠꄡꄢꄣꄤꄥꄦꄧꄨꄩꄪꄫꄬꄭꄮꄯꄰꄱꄲꄳꄴꄵ"
    u8"ꄶꄷꄸꄹꄺꄻꄼꄽꄾꄿꅀꅁꅂꅃꅄꅅꅆꅇꅈꅉꅊꅋꅌꅍꅎꅏꅐꅑꅒꅓꅔꅕ"
    u8"ꅖꅗꅘꅙꅚꅛꅜꅝꅞꅟꅠꅡꅢꅣꅤꅥꅦꅧꅨꅩꅪꅫꅬꅭꅮꅯꅰꅱꅲꅳꅴꅵ"
    u8"ꅶꅷꅸꅹꅺꅻꅼꅽꅾꅿꆀꆁꆂꆃꆄꆅꆆꆇꆈꆉꆊꆋꆌꆍꆎꆏꆐꆑꆒꆓꆔꆕ"
    u8"ꆖꆗꆘꆙꆚꆛꆜꆝꆞꆟꆠꆡꆢꆣꆤꆥꆦꆧꆨꆩꆪꆫꆬꆭꆮꆯꆰꆱꆲꆳꆴꆵ"
    u8"ꆶꆷꆸꆹꆺꆻꆼꆽꆾꆿꇀꇁꇂꇃꇄꇅꇆꇇꇈꇉꇊꇋꇌꇍꇎꇏꇐꇑꇒꇓꇔꇕ"
    u8"ꇖꇗꇘꇙꇚꇛꇜꇝꇞꇟꇠꇡꇢꇣꇤꇥꇦꇧꇨꇩꇪꇫꇬꇭꇮꇯꇰꇱꇲꇳꇴꇵ"
    u8"ꇶꇷꇸꇹꇺꇻꇼꇽꇾꇿꈀꈁꈂꈃꈄꈅꈆꈇꈈꈉꈊꈋꈌꈍꈎꈏꈐꈑꈒꈓꈔꈕ"
    u8"ꈖꈗꈘꈙꈚꈛꈜꈝꈞꈟꈠꈡꈢꈣꈤꈥꈦꈧꈨꈩꈪꈫꈬꈭꈮꈯꈰꈱꈲꈳꈴꈵ"
    u8"ꈶꈷꈸꈹꈺꈻꈼꈽꈾꈿꉀꉁꉂꉃꉄꉅꉆꉇꉈꉉꉊꉋꉌꉍꉎꉏꉐꉑꉒꉓꉔꉕ"
    u8"ꉖꉗꉘꉙꉚꉛꉜꉝꉞꉟꉠꉡꉢꉣꉤꉥꉦꉧꉨꉩꉪꉫꉬꉭꉮꉯꉰꉱꉲꉳꉴꉵ"
    u8"ꉶꉷꉸꉹꉺꉻꉼꉽꉾꉿꊀꊁꊂꊃꊄꊅꊆꊇꊈꊉꊊꊋꊌꊍꊎꊏꊐꊑꊒꊓꊔꊕ"
    u8"ꊖꊗꊘꊙꊚꊛꊜꊝꊞꊟꊠꊡꊢꊣꊤꊥꊦꊧꊨꊩꊪꊫꊬꊭꊮꊯꊰꊱꊲꊳꊴꊵ"
    u8"ꊶꊷꊸꊹꊺꊻꊼꊽꊾꊿꋀꋁꋂꋃꋄꋅꋆꋇꋈꋉꋊꋋꋌꋍꋎꋏꋐꋑꋒꋓꋔꋕ"
    u8"ꋖꋗꋘꋙꋚꋛꋜꋝꋞꋟꋠꋡꋢꋣꋤꋥꋦꋧꋨꋩꋪꋫꋬꋭꋮꋯꋰꋱꋲꋳꋴꋵ"
    u8"ꋶꋷꋸꋹꋺꋻꋼꋽꋾꋿꌀꌁꌂꌃꌄꌅꌆꌇꌈꌉꌊꌋꌌꌍꌎꌏꌐꌑꌒꌓꌔꌕ"
    u8"ꌖꌗꌘꌙꌚꌛꌜꌝꌞꌟꌠꌡꌢꌣꌤꌥꌦꌧꌨꌩꌪꌫꌬꌭꌮꌯꌰꌱꌲꌳꌴꌵ"
    u8"ꌶꌷꌸꌹꌺꌻꌼꌽꌾꌿꍀꍁꍂꍃꍄꍅꍆꍇꍈꍉꍊꍋꍌꍍꍎꍏꍐꍑꍒꍓꍔꍕ"
    u8"ꍖꍗꍘꍙꍚꍛꍜꍝꍞꍟꍠꍡꍢꍣꍤꍥꍦꍧꍨꍩꍪꍫꍬꍭꍮꍯꍰꍱꍲꍳꍴꍵ"
    u8"ꍶꍷꍸꍹꍺꍻꍼꍽꍾꍿꎀꎁꎂꎃꎄꎅꎆꎇꎈꎉꎊꎋꎌꎍꎎꎏꎐꎑꎒꎓꎔꎕ"
    u8"ꎖꎗꎘꎙꎚꎛꎜꎝꎞꎟꎠꎡꎢꎣꎤꎥꎦꎧꎨꎩꎪꎫꎬꎭꎮꎯꎰꎱꎲꎳꎴꎵ"
    u8"ꎶꎷꎸꎹꎺꎻꎼꎽꎾꎿꏀꏁꏂꏃꏄꏅꏆꏇꏈꏉꏊꏋꏌꏍꏎꏏꏐꏑꏒꏓꏔꏕ"
    u8"ꏖꏗꏘꏙꏚꏛꏜꏝꏞꏟꏠꏡꏢꏣꏤꏥꏦꏧꏨꏩꏪꏫꏬꏭꏮꏯꏰꏱꏲꏳꏴꏵ"
    u8"ꏶꏷꏸꏹꏺꏻꏼꏽꏾꏿꐀꐁꐂꐃꐄꐅꐆꐇꐈꐉꐊꐋꐌꐍꐎꐏꐐꐑꐒꐓꐔꐕ"
    u8"ꐖꐗꐘꐙꐚꐛꐜꐝꐞꐟꐠꐡꐢꐣꐤꐥꐦꐧꐨꐩꐪꐫꐬꐭꐮꐯꐰꐱꐲꐳꐴꐵ"
    u8"ꐶꐷꐸꐹꐺꐻꐼꐽꐾꐿꑀꑁꑂꑃꑄꑅꑆꑇꑈꑉꑊꑋꑌꑍꑎꑏꑐꑑꑒꑓꑔꑕ"
    u8"ꑖꑗꑘꑙꑚꑛꑜꑝꑞꑟꑠꑡꑢꑣꑤꑥꑦꑧꑨꑩꑪꑫꑬꑭꑮꑯꑰꑱꑲꑳꑴꑵ"
    u8"ꑶꑷꑸꑹꑺꑻꑼꑽꑾꑿꒀꒁꒂꒃꒄꒅꒆꒇꒈꒉꒊꒋꒌꀀꀁꀂꀃꀄꀅꀆꀇꀈ"
    u8"ꀉꀊꀋꀌꀍꀎꀏꀐꀑꀒꀓꀔꀕꀖꀗꀘꀙꀚꀛꀜꀝꀞꀟꀠꀡꀢꀣꀤꀥꀦꀧꀨ"
    u8"ꀩꀪꀫꀬꀭꀮꀯꀰꀱꀲꀳꀴꀵꀶꀷꀸꀹꀺꀻꀼꀽꀾꀿꁀꁁꁂꁃꁄꁅꁆꁇꁈ"
    u8"ꁉꁊꁋꁌꁍꁎꁏꁐꁑꁒꁓꁔꁕꁖꁗꁘꁙꁚꁛꁜꁝꁞꁟꁠꁡꁢꁣꁤꁥꁦꁧꁨ"
    u8"ꁩꁪꁫꁬꁭꁮꁯꁰꁱꁲꁳꁴꁵꁶꁷꁸꁹꁺꁻꁼꁽꁾꁿꂀꂁꂂꂃꂄꂅꂆꂇꂈ"
    u8"ꂉꂊꂋꂌꂍꂎꂏꂐꂑꂒꂓꂔꂕꂖꂗꂘꂙꂚꂛꂜꂝꂞꂟꂠꂡꂢꂣꂤꂥꂦꂧꂨ"
    u8"ꂩꂪꂫꂬꂭꂮꂯꂰꂱꂲꂳꂴꂵꂶꂷꂸꂹꂺꂻꂼꂽꂾꂿꃀꃁꃂꃃꃄꃅꃆꃇꃈ"
    u8"ꃉꃊꃋꃌꃍꃎꃏꃐꃑꃒꃓꃔꃕꃖꃗꃘꃙꃚꃛꃜꃝꃞꃟꃠꃡꃢꃣꃤꃥꃦꃧꃨ"
    u8"ꃩꃪꃫꃬꃭꃮꃯꃰꃱꃲꃳꃴꃵꃶꃷꃸꃹꃺꃻꃼꃽꃾꃿꄀꄁꄂꄃꄄꄅꄆꄇꄈ"
    u8"ꄉꄊꄋꄌꄍꄎꄏꄐꄑꄒꄓꄔꄕꄖꄗꄘꄙꄚꄛꄜꄝꄞꄟꄠꄡꄢꄣꄤꄥꄦꄧꄨ"
    u8"ꄩꄪꄫꄬꄭꄮꄯꄰꄱꄲꄳꄴꄵꄶꄷꄸꄹꄺꄻꄼꄽꄾꄿꅀꅁꅂꅃꅄꅅꅆꅇꅈ"
    u8"ꅉꅊꅋꅌꅍꅎꅏꅐꅑꅒꅓꅔꅕꅖꅗꅘꅙꅚꅛꅜꅝꅞꅟꅠꅡꅢꅣꅤꅥꅦꅧꅨ"
    u8"ꅩꅪꅫꅬꅭꅮꅯꅰꅱꅲꅳꅴꅵꅶꅷꅸꅹꅺꅻꅼꅽꅾꅿꆀꆁꆂꆃꆄꆅꆆꆇꆈ"
    u8"ꆉꆊꆋꆌꆍꆎꆏꆐꆑꆒꆓꆔꆕꆖꆗꆘꆙꆚꆛꆜꆝꆞꆟꆠꆡꆢꆣꆤꆥꆦꆧꆨ"
    u8"ꆩꆪꆫꆬꆭꆮꆯꆰꆱꆲꆳꆴꆵꆶꆷꆸꆹꆺꆻꆼꆽꆾꆿꇀꇁꇂꇃꇄꇅꇆꇇꇈ"
    u8"ꇉꇊꇋꇌꇍꇎꇏꇐꇑꇒꇓꇔꇕꇖꇗꇘꇙꇚꇛꇜꇝꇞꇟꇠꇡꇢꇣꇤꇥꇦꇧꇨ"
    u8"ꇩꇪꇫꇬꇭꇮꇯꇰꇱꇲꇳꇴꇵꇶꇷꇸꇹꇺꇻꇼꇽꇾꇿꈀꈁꈂꈃꈄꈅꈆꈇꈈ"
    u8"ꈉꈊꈋꈌꈍꈎꈏꈐꈑꈒꈓꈔꈕꈖꈗꈘꈙꈚꈛꈜꈝꈞꈟꈠꈡꈢꈣꈤꈥꈦꈧꈨ"
    u8"ꈩꈪꈫꈬꈭꈮꈯꈰꈱꈲꈳꈴꈵꈶꈷꈸꈹꈺꈻꈼꈽꈾꈿꉀꉁꉂꉃꉄꉅꉆꉇꉈ"
    u8"ꉉꉊꉋꉌꉍꉎꉏꉐꉑꉒꉓꉔꉕꉖꉗꉘꉙꉚꉛꉜꉝꉞꉟꉠꉡꉢꉣꉤꉥꉦꉧꉨ"
    u8"ꉩꉪꉫꉬꉭꉮꉯꉰꉱꉲꉳꉴꉵꉶꉷꉸꉹꉺꉻꉼꉽꉾꉿꊀꊁꊂꊃꊄꊅꊆꊇꊈ"
    u8"ꊉꊊꊋꊌꊍꊎꊏꊐꊑꊒꊓꊔꊕꊖꊗꊘꊙꊚꊛꊜꊝꊞꊟꊠꊡꊢꊣꊤꊥꊦꊧꊨ"
    u8"ꊩꊪꊫꊬꊭꊮꊯꊰꊱꊲꊳꊴꊵꊶꊷꊸꊹꊺꊻꊼꊽꊾꊿꋀꋁꋂꋃꋄꋅꋆꋇꋈ"
    u8"ꋉꋊꋋꋌꋍꋎꋏꋐꋑꋒꋓꋔꋕꋖꋗꋘꋙꋚꋛꋜꋝꋞꋟꋠꋡꋢꋣꋤꋥꋦꋧꋨ"
    u8"ꋩꋪꋫꋬꋭꋮꋯꋰꋱꋲꋳꋴꋵꋶꋷꋸꋹꋺꋻꋼꋽꋾꋿꌀꌁꌂꌃꌄꌅꌆꌇꌈ"
    u8"ꌉꌊꌋꌌꌍꌎꌏꌐꌑꌒꌓꌔꌕꌖꌗꌘꌙꌚꌛꌜꌝꌞꌟꌠꌡꌢꌣꌤꌥꌦꌧꌨ"
    u8"ꌩꌪꌫꌬꌭꌮꌯꌰꌱꌲꌳꌴꌵꌶꌷꌸꌹꌺꌻꌼꌽꌾꌿꍀꍁꍂꍃꍄꍅꍆꍇꍈ"
    u8"ꍉꍊꍋꍌꍍꍎꍏꍐꍑꍒꍓꍔꍕꍖꍗꍘꍙꍚꍛꍜꍝꍞꍟꍠꍡꍢꍣꍤꍥꍦꍧꍨ"
    u8"ꍩꍪꍫꍬꍭꍮꍯꍰꍱꍲꍳꍴꍵꍶꍷꍸꍹꍺꍻꍼꍽꍾꍿꎀꎁꎂꎃꎄꎅꎆꎇꎈ"
    u8"ꎉꎊꎋꎌꎍꎎꎏꎐꎑꎒꎓꎔꎕꎖꎗꎘꎙꎚꎛꎜꎝꎞꎟꎠꎡꎢꎣꎤꎥꎦꎧꎨ"
    u8"ꎩꎪꎫꎬꎭꎮꎯꎰꎱꎲꎳꎴꎵꎶꎷꎸꎹꎺꎻꎼꎽꎾꎿꏀꏁꏂꏃꏄꏅꏆꏇꏈ"
    u8"ꏉꏊꏋꏌꏍꏎꏏꏐꏑꏒꏓꏔꏕꏖꏗꏘꏙꏚꏛꏜꏝꏞꏟꏠꏡꏢꏣꏤꏥꏦꏧꏨ"
    u8"ꏩꏪꏫꏬꏭꏮꏯꏰꏱꏲꏳꏴꏵꏶꏷꏸꏹꏺꏻꏼꏽꏾꏿꐀꐁꐂꐃꐄꐅꐆꐇꐈ"
    u8"ꐉꐊꐋꐌꐍꐎꐏꐐꐑꐒꐓꐔꐕꐖꐗꐘꐙꐚꐛꐜꐝꐞꐟꐠꐡꐢꐣꐤꐥꐦꐧꐨ"
    u8"ꐩꐪꐫꐬꐭꐮꐯꐰꐱꐲꐳꐴꐵꐶꐷꐸꐹꐺꐻꐼꐽꐾꐿꑀꑁꑂꑃꑄꑅꑆꑇꑈ"
    u8"ꑉꑊꑋꑌꑍꑎꑏꑐꑑꑒꑓꑔꑕꑖꑗꑘꑙꑚꑛꑜꑝꑞꑟꑠꑡꑢꑣꑤꑥꑦꑧꑨ"
    u8"ꑩꑪꑫꑬꑭꑮꑯꑰꑱꑲꑳꑴꑵꑶꑷꑸꑹꑺꑻꑼꑽꑾꑿꒀꒁꒂꒃꒄꒅꒆꒇꒈ"
    u8"ꒉꒊꒋꒌ黰黱黲黳黴黵黶黷黸黹黺黻黼黽黾𝌀𝌁𝌂𝌃𝌄𝌅𝌆𝌇𝌈𝌉𝌊𝌋𝌌"
    u8"𝌍𝌎𝌏𞋀‎𞋁‎𞋂‎𞋃‎𞋄‎𞋅‎𞋆‎𞋇‎𞋈‎𞋉‎𞋊‎𞋋‎𞋌‎𞋍‎𞋎"
    u8"‎𞋏‎𞋐‎𞋑‎𞋒‎𞋓‎𞋔‎𞋕‎𞋖‎𞋗‎𞋘‎𞋙‎𞋚‎𞋛‎𞋜‎𞋝‎𞋞"
    u8"‎𞋟‎𞋠‎𞋡‎𞋢‎𞋣‎𞋤‎𞋥‎𞋦‎𞋧‎𞋨‎𞋩‎𞋪‎𞋫‎𞋬‎𞋭‎𞋮"
    u8"‎𞋯‎𞋰‎𞋱‎𞋲‎𞋳‎𞋴‎𞋵‎𞋶‎𞋷‎𞋸‎𞋹‎𞋿︐︑︒︓︔︕︖︗"
    u8"︘︙𝌐𝌑𝌒𝌓𝌔𝌕𝌖𝌗𝌘𝌙𝌚𝌛𝌜𝌝𝌞𝌟𝌠𝌡𝌢𝌣𝌤𝌥𝌦𝌧𝌨𝌩𝌪𝌫𝌬𝌭"
    u8"𝌮𝌯𝌰𝌱𝌲𝌳𝌴𝌵𝌶𝌷𝌸𝌹𝌺𝌻𝌼𝌽𝌾𝌿𝍀𝍁𝍂𝍃𝍄𝍅𝍆𝍇𝍈𝍉𝍊𝍋𝍌𝍍"
    u8"𝍎𝍏𝍐𝍑𝍒𝍓𝍔𝍕𝍖黿鼀鼁鼂鼃鼄鼅鼆鼇鼈鼉鼊鼋鼌鼍鼎ꓐꓑꓒꓓꓔꓕꓖ"
    u8"ꓗꓘꓙꓚꓛꓜꓝꓞꓟꓠꓡꓢꓣꓤꓥꓦꓧꓨꓩꓪꓫꓬꓭꓮꓯꓰꓱꓲꓳꓴꓵꓶ"
    u8"ꓷꓸꓹꓺꓻꓼꓽ꓾꓿鼏鼐鼑鼒鼓鼔鼕鼖鼗鼘鼙鼚鼛鼜鼝鼞鼟鼠鼡鼢鼣鼤鼥"
    u8"鼦鼧鼨鼩鼪鼫鼬鼭鼮鼯鼰鼱鼲鼳鼴鼵鼶鼷鼸鼹鼺鼻鼼鼽鼾鼿齀齁齂齃齄齅"
    u8"齆齇齈齉齊齋齌齍齎齏齐齑齒齓齔齕齖齗齘齙齚齛齜齝齞齟齠齡齢齣齤齥"
    u8"齦齧齨齩齪齫齬齭齮齯齰齱齲齳齴齵齶齷齸齹齺齻齼齽齾齿龀龁龂龃龄龅"
    u8"龆龇龈龉龊龋龌龍龎龏龐龑龒龓龔龕龖龗龘龙龚龛龜龝龞龟龠龡龢龣龤龥"
    u8"龦龧龨龩龪龫龬龭龮龯龰龱龲龳龴龵龶龷龸龹龺龻龼龽龾龿鿀鿁鿂鿃鿄鿅"
    u8"鿆鿇鿈鿉鿊鿋鿌鿍鿎鿏鿐鿑鿒鿓鿔鿕鿖鿗鿘鿙鿚鿛鿜鿝鿞鿟鿠鿡鿢鿣鿤鿥"
    u8"鿦鿧鿨鿩鿪鿫鿬鿭鿮鿯鿰鿱鿲鿳鿴鿵鿶鿷鿸鿹鿺鿻鿼鿽鿾鿿⺀⺁⺂⺃⺄⺅"
    u8"⺆⺇⺈⺉⺊⺋⺌⺍⺎⺏⺐⺑⺒⺓⺔⺕⺖⺗⺘⺙⺛⺜⺝⺞⺟⺠⺡⺢⺣⺤⺥⺦"
    u8"⺧⺨⺩⺪⺫⺬⺭⺮⺯⺰⺱⺲⺳⺴⺵⺶⺷⺸⺹⺺⺻⺼⺽⺾⺿⻀⻁⻂⻃⻄⻅⻆"
    u8"⻇⻈⻉⻊⻋⻌⻍⻎⻏⻐⻑⻒⻓⻔⻕⻖⻗⻘⻙⻚⻛⻜⻝⻞⻟⻠⻡⻢⻣⻤⻥⻦"
    u8"⻧⻨⻩⻪⻫⻬⻭⻮⻯⻰⻱⻲⻳㆐㆑㆒㆓㆔㆕㆖㆗㆘㆙㆚㆛㆜㆝㆞𚿰𚿱𚿲𚿳"
    u8"𚿵𚿶𚿷𚿸𚿹𚿺𚿻𚿽𚿾ㇰㇱㇲㇳㇴㇵㇶㇷㇸㇹㇺㇻㇼㇽㇾ𘬀‎𘬁‎𘬂‎𘬃‎"
    u8"𘬄‎𘬅‎𘬆‎𘬇‎𘬈‎𘬉‎𘬊‎𘬋‎𘬌‎𘬍‎𘬎‎𘬏‎𘬐‎𘬑‎𘬒‎𘬓‎"
    u8"𘬔‎𘬕‎𘬖‎𘬗‎𘬘‎𘬙‎𘬚‎𘬛‎𘬜‎𘬝‎𘬞‎𘬟‎𘬠‎𘬡‎𘬢‎𘬣‎"
    u8"𘬤‎𘬥‎𘬦‎𘬧‎𘬨‎𘬩‎𘬪‎𘬫‎𘬬‎𘬭‎𘬮‎𘬯‎𘬰‎𘬱‎𘬲‎𘬳‎"
    u8"𘬴‎𘬵‎𘬶‎𘬷‎𘬸‎𘬹‎𘬺‎𘬻‎𘬼‎𘬽‎𘬾‎𘬿‎𘭀‎𘭁‎𘭂‎𘭃‎"
    u8"𘭄‎𘭅‎𘭆‎𘭇‎𘭈‎𘭉‎𘭊‎𘭋‎𘭌‎𘭍‎𘭎‎𘭏‎𘭐‎𘭑‎𘭒‎𘭓‎"
    u8"𘭔‎𘭕‎𘭖‎𘭗‎𘭘‎𘭙‎𘭚‎𘭛‎𘭜‎𘭝‎𘭞‎𘭟‎𘭠‎𘭡‎𘭢‎𘭣‎"
    u8"𘭤‎𘭥‎𘭦‎𘭧‎𘭨‎𘭩‎𘭪‎𘭫‎𘭬‎𘭭‎𘭮‎𘭯‎𘭰‎𘭱‎𘭲‎𘭳‎"
    u8"𘭴‎𘭵‎𘭶‎𘭷‎𘭸‎𘭹‎𘭺‎𘭻‎𘭼‎𘭽‎𘭾‎𘭿‎𘮀‎𘮁‎𘮂‎𘮃‎"
    u8"𘮄‎𘮅‎𘮆‎𘮇‎𘮈‎𘮉‎𘮊‎𘮋‎𘮌‎𘮍‎𘮎‎𘮏‎𘮐‎𘮑‎𘮒‎𘮓‎"
    u8"𘮔‎𘮕‎𘮖‎𘮗‎𘮘‎𘮙‎𘮚‎𘮛‎𘮜‎𘮝‎𘮞‎𘮟‎𘮠‎𘮡‎𘮢‎𘮣‎"
    u8"𘮤‎𘮥‎𘮦‎𘮧‎𘮨‎𘮩‎𘮪‎𘮫‎𘮬‎𘮭‎𘮮‎𘮯‎𘮰‎𘮱‎𘮲‎𘮳‎"
    u8"𘮴‎𘮵‎𘮶‎𘮷‎𘮸‎𘮹‎𘮺‎𘮻‎𘮼‎𘮽‎𘮾‎𘮿‎𘯀‎𘯁‎𘯂‎𘯃‎"
    u8"𘯄‎𘯅‎𘯆‎𘯇‎𘯈‎𘯉‎𘯊‎𘯋‎𘯌‎𘯍‎𘯎‎𘯏‎𘯐‎𘯑‎𘯒‎𘯓‎"
    u8"𘯔‎𘯕‎𘯖‎𘯗‎𘯘‎𘯙‎𘯚‎𘯛‎𘯜‎𘯝‎𘯞‎𘯟‎𘯠‎𘯡‎𘯢‎𘯣‎"
    u8"𘯤‎𘯥‎𘯦‎𘯧‎𘯨‎𘯩‎𘯪‎𘯫‎𘯬‎𘯭‎𘯮‎𘯯‎𘯰‎𘯱‎𘯲‎𘯳‎"
    u8"𘯴‎𘯵‎𘯶‎𘯷‎𘯸‎𘯹‎𘯺‎𘯻‎𘯼‎𘯽‎𘯾‎𘯿‎𘰀‎𘰁‎𘰂‎𘰃‎"
    u8"𘰄‎𘰅‎𘰆‎𘰇‎𘰈‎𘰉‎𘰊‎𘰋‎𘰌‎𘰍‎𘰎‎𘰏‎𘰐‎𘰑‎𘰒‎𘰓‎"
    u8"𘰔‎𘰕‎𘰖‎𘰗‎𘰘‎𘰙‎𘰚‎𘰛‎𘰜‎𘰝‎𘰞‎𘰟‎𘰠‎𘰡‎𘰢‎𘰣‎"
    u8"𘰤‎𘰥‎𘰦‎𘰧‎𘰨‎𘰩‎𘰪‎𘰫‎𘰬‎𘰭‎𘰮‎𘰯‎𘰰‎𘰱‎𘰲‎𘰳‎"
    u8"𘰴‎𘰵‎𘰶‎𘰷‎𘰸‎𘰹‎𘰺‎𘰻‎𘰼‎𘰽‎𘰾‎𘰿‎𘱀‎𘱁‎𘱂‎𘱃‎"
    u8"𘱄‎𘱅‎𘱆‎𘱇‎𘱈‎𘱉‎𘱊‎𘱋‎𘱌‎𘱍‎𘱎‎𘱏‎𘱐‎𘱑‎𘱒‎𘱓‎"
    u8"𘱔‎𘱕‎𘱖‎𘱗‎𘱘‎𘱙‎𘱚‎𘱛‎𘱜‎𘱝‎𘱞‎𘱟‎𘱠‎𘱡‎𘱢‎𘱣‎"
    u8"𘱤‎𘱥‎𘱦‎𘱧‎𘱨‎𘱩‎𘱪‎𘱫‎𘱬‎𘱭‎𘱮‎𘱯‎𘱰‎𘱱‎𘱲‎𘱳‎"
    u8"𘱴‎𘱵‎𘱶‎𘱷‎𘱸‎𘱹‎𘱺‎𘱻‎𘱼‎𘱽‎𘱾‎𘱿‎𘲀‎𘲁‎𘲂‎𘲃‎"
    u8"𘲄‎𘲅‎𘲆‎𘲇‎𘲈‎𘲉‎𘲊‎𘲋‎𘲌‎𘲍‎𘲎‎𘲏‎𘲐‎𘲑‎𘲒‎𘲓‎"
    u8"𘲔‎𘲕‎𘲖‎𘲗‎𘲘‎𘲙‎𘲚‎𘲛‎𘲜‎𘲝‎𘲞‎𘲟‎𘲠‎𘲡‎𘲢‎𘲣‎"
    u8"𘲤‎𘲥‎𘲦‎𘲧‎𘲨‎𘲩‎𘲪‎𘲫‎𘲬‎𘲭‎𘲮‎𘲯‎𘲰‎𘲱‎𘲲‎𘲳‎"
    u8"𘲴‎𘲵‎𘲶‎𘲷‎𘲸‎𘲹‎𘲺‎𘲻‎𘲼‎𘲽‎𘲾‎𘲿‎𘳀‎𘳁‎𘳂‎𘳃‎"
    u8"𘳄‎𘳅‎𘳆‎𘳇‎𘳈‎𘳉‎𘳊‎𘳋‎𘳌‎𘳍‎𘳎‎𘳏‎𘳐‎𘳑‎𘳒‎𘳓‎"
    u8"𘳔‎𘳕‎ㇿ𛀀𛀁𛀂𛀃𛀄𛀅𛀆𛀇𛀈𛀉𛀊𛀋𛀌𛀍𛀎𛀏𛀐𛀑𛀒𛀓𛀔𛀕𛀖𛀗𛀘𛀙𛀚"
    u8"𛀛𛀜𛀝𛀞𛀟𛀠𛀡𛀢𛀣𛀤𛀥𛀦𛀧𛀨𛀩𛀪𛀫𛀬𛀭𛀮𛀯𛀰𛀱𛀲𛀳𛀴𛀵𛀶𛀷𛀸𛀹𛀺"
    u8"𛀻𛀼𛀽𛀾𛀿𛁀𛁁𛁂𛁃𛁄𛁅𛁆𛁇𛁈𛁉𛁊𛁋𛁌𛁍𛁎𛁏𛁐𛁑𛁒𛁓𛁔𛁕𛁖𛁗𛁘𛁙𛁚"
    u8"𛁛𛁜𛁝𛁞𛁟𛁠𛁡𛁢𛁣𛁤𛁥𛁦𛁧𛁨𛁩𛁪𛁫𛁬𛁭𛁮𛁯𛁰𛁱𛁲𛁳𛁴𛁵𛁶𛁷𛁸𛁹𛁺"
    u8"𛁻𛁼𛁽𛁾𛁿𛂀𛂁𛂂𛂃𛂄𛂅𛂆𛂇𛂈𛂉𛂊𛂋𛂌𛂍𛂎𛂏𛂐𛂑𛂒𛂓𛂔𛂕𛂖𛂗𛂘𛂙𛂚"
    u8"𛂛𛂜𛂝𛂞𛂟𛂠𛂡𛂢𛂣𛂤𛂥𛂦𛂧𛂨𛂩𛂪𛂫𛂬𛂭𛂮𛂯𛂰𛂱𛂲𛂳𛂴𛂵𛂶𛂷𛂸𛂹𛂺"
    u8"𛂻𛂼𛂽𛂾𛂿𛃀𛃁𛃂𛃃𛃄𛃅𛃆𛃇𛃈𛃉𛃊𛃋𛃌𛃍𛃎𛃏𛃐𛃑𛃒𛃓𛃔𛃕𛃖𛃗𛃘𛃙𛃚"
    u8"𛃛𛃜𛃝𛃞𛃟𛃠𛃡𛃢𛃣𛃤𛃥𛃦𛃧𛃨𛃩𛃪𛃫𛃬𛃭𛃮𛃯𛃰𛃱𛃲𛃳𛃴𛃵𛃶𛃷𛃸𛃹𛃺"
    u8"𛃻𛃼𛃽𛃾𛃿𛄀𛄁𛄂𛄃𛄄𛄅𛄆𛄇𛄈𛄉𛄊𛄋𛄌𛄍𛄎𛄏𛄐𛄑𛄒𛄓𛄔𛄕𛄖𛄗𛄘𛄙𛄚"
    u8"𛄛𛄜𛄝𛄞𛄟𛄠𛄡𛄢゠ァアィイゥウェエォオカガキギクグケゲコゴサザシ"
    u8"ジスズセゼソゾタダチヂッツヅテデトドナニヌネノハバパヒビピフブプ"
    u8"ヘベペホボポマミムメモャヤュユョヨラリルレロヮワヰヱヲンヴヵヶヷ"
    u8"ヸヹヺ・ーヽヾヿ⼀⼁⼂⼃⼄⼅⼆⼇⼈⼉⼊⼋⼌⼍⼎⼏⼐⼑⼒⼓⼔⼕⼖⼗"
    u8"⼘⼙⼚⼛⼜⼝⼞⼟⼠⼡⼢⼣⼤⼥⼦⼧⼨⼩⼪⼫⼬⼭⼮⼯⼰⼱⼲⼳⼴⼵⼶⼷"
    u8"⼸⼹⼺⼻⼼⼽⼾⼿⽀⽁⽂⽃⽄⽅⽆⽇⽈⽉⽊⽋⽌⽍⽎⽏⽐⽑⽒⽓⽔⽕⽖⽗"
    u8"⽘⽙⽚⽛⽜⽝⽞⽟⽠⽡⽢⽣⽤⽥⽦⽧⽨⽩⽪⽫⽬⽭⽮⽯⽰⽱⽲⽳⽴⽵⽶⽷"
    u8"⽸⽹⽺⽻⽼⽽⽾⽿⾀⾁⾂⾃⾄⾅⾆⾇⾈⾉⾊⾋⾌⾍⾎⾏⾐⾑⾒⾓⾔⾕⾖⾗"
    u8"⾘⾙⾚⾛⾜⾝⾞⾟⾠⾡⾢⾣⾤⾥⾦⾧⾨⾩⾪⾫⾬⾭⾮⾯⾰⾱⾲⾳⾴⾵⾶⾷"
    u8"⾸⾹⾺⾻⾼⾽⾾⾿⿀⿁⿂⿃⿄⿅⿆⿇⿈⿉⿊⿋⿌⿍⿎⿏⿐⿑⿒⿓⿔⿕㆟𖿠"
    u8"𖿡𖿢𖿣𖿰𖿱⿰⿱⿲⿳⿴⿵⿶⿷⿸⿹⿺⿻ぁあぃいぅうぇえぉおかがきぎく"
    u8"ぐけげこごさざしじすずせぜそぞただちぢっつづてでとどなにぬねのは"
    u8"ばぱひびぴふぶぷへべぺほぼぽまみむめもゃやゅゆょよらりるれろゎわ"
    u8"ゐゑをんゔゕゖ゙゚゛゜ゝゞゟㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍ"
    u8"ㅎㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻ"
    u8"ㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ！＂＃＄％＆＇（）＊＋，－．／"
    u8"０１２３４５６７８９：；＜＝＞？＠ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯ"
    u8"ＰＱＲＳＴＵＶＷＸＹＺ［＼］＾＿｀ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏ"
    u8"ｐｑｒｓｔｕｖｗｘｙｚ｛｜｝～｟｠｡｢｣､･ｦｧｨｩｪｫｬｭｮｯ"
    u8"ｰｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏ"
    u8"ﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝﾞﾟﾡﾢﾣﾤﾥﾦﾧﾨﾩﾪﾫﾬﾭﾮﾯﾰ"
    u8"ﾱﾲﾳﾴﾵﾶﾷﾸﾹﾺﾻﾼﾽﾾￂￃￄￅￆￇￊￋￌￍￎￏￒￓￔￕￖￗ"
    u8"ￚￛￜ￠￡￢￣￤￥￦￨￩￪￫￬￭￮🈀🈁🈂🈐🈑🈒🈓🈔🈕🈖🈗🈘🈙🈚🈛"
    u8"🈜🈝🈞🈟🈠🈡🈢🈣🈤🈥🈦🈧🈨🈩🈪🈫🈬🈭🈮🈯🈰🈱🈲🈳🈴🈵🈶🈷🈸🈹🈺🈻"
    u8"🉀🉁🉂🉃🉄🉅🉆🉇🉈🉐🉑🉠🉡🉢🉣🉤🉥㈀㈁㈂㈃㈄㈅㈆㈇㈈㈉㈊㈋㈌㈍㈎"
    u8"㈏㈐㈑㈒㈓㈔㈕㈖㈗㈘㈙㈚㈛㈜㈝㈞㈠㈡㈢㈣㈤㈥㈦㈧㈨㈩㈪㈫㈬㈭㈮㈯"
    u8"㈰㈱㈲㈳㈴㈵㈶㈷㈸㈹㈺㈻㈼㈽㈾㈿㉀㉁㉂㉃㉄㉅㉆㉇㉈㉉㉊㉋㉌㉍㉎㉏"
    u8"㉐㉑㉒㉓㉔㉕㉖㉗㉘㉙㉚㉛㉜㉝㉞㉟㉠㉡㉢㉣㉤㉥㉦㉧㉨㉩㉪㉫㉬㉭㉮㉯"
    u8"㉰㉱㉲㉳㉴㉵㉶㉷㉸㉹㉺㉻㉼㉽㉾㉿㊀㊁㊂㊃㊄㊅㊆㊇㊈㊉㊊㊋㊌㊍㊎㊏"
    u8"㊐㊑㊒㊓㊔㊕㊖㊗㊘㊙㊚㊛㊜㊝㊞㊟㊠㊡㊢㊣㊤㊥㊦㊧㊨㊩㊪㊫㊬㊭㊮㊯"
    u8"㊰㊱㊲㊳㊴㊵㊶㊷㊸㊹㊺㊻㊼㊽㊾㊿㋀㋁㋂㋃㋄㋅㋆㋇㋈㋉㋊㋋㋌㋍㋎㋏"
    u8"㋐㋑㋒㋓㋔㋕㋖㋗㋘㋙㋚㋛㋜㋝㋞㋟㋠㋡㋢㋣㋤㋥㋦㋧㋨㋩㋪㋫㋬㋭㋮㋯"
    u8"㋰㋱㋲㋳㋴㋵㋶㋷㋸㋹㋺㋻㋼㋽㋾㋿🄀🄁🄂🄃🄄🄅🄆🄇🄈🄉🄊🄋🄌🄍🄎🄏"
    u8"🄐🄑🄒🄓🄔🄕🄖🄗🄘🄙🄚🄛🄜🄝🄞🄟🄠🄡🄢🄣🄤🄥🄦🄧🄨🄩🄪🄫🄬🄭🄮🄯"
    u8"🄰🄱🄲🄳🄴🄵🄶🄷🄸🄹🄺🄻🄼🄽🄾🄿🅀🅁🅂🅃🅄🅅🅆🅇🅈🅉🅊🅋🅌🅍🅎🅏"
    u8"🅐🅑🅒🅓🅔🅕🅖🅗🅘🅙🅚🅛🅜🅝🅞🅟🅠🅡🅢🅣🅤🅥🅦🅧🅨🅩🅪🅫🅬🅭🅮🅯"
    u8"🅰🅱🅲🅳🅴🅵🅶🅷🅸🅹🅺🅻🅼🅽🅾🅿🆀🆁🆂🆃🆄🆅🆆🆇🆈🆉🆊🆋🆌🆍🆎🆏"
    u8"🆐🆑🆒🆓🆔🆕🆖🆗🆘🆙🆚🆛🆜🆝🆞🆟🆠🆡🆢🆣🆤🆥🆦🆧🆨🆩🆪🆫🆬🆭🇦🇧"
    u8"🇨🇩🇪🇫🇬🇭🇮🇯🇰🇱🇲🇳🇴🇵🇶🇷🇸🇹🇺🇻🇼🇽🇾🇿𝍠𝍡𝍢𝍣𝍤𝍥𝍦𝍧"
    u8"𝍨𝍩𝍪𝍫𝍬𝍭𝍮𝍯𝍰𝍱𝍲𝍳𝍴𝍵𝍶𝍷𝍸、。〃〄々〆〇〈〉《》「」『』"
    u8"【】〒〓〔〕〖〗〘〙〚〛〜〝〞〟〠〡〢〣〤〥〦〧〨〩〪〭〮〯〫〬"
    u8"〰〱〲〳〴〵〶〷〸〹〺〻〼〽〾〿㇀㇁㇂㇃㇄㇅㇆㇇㇈㇉㇊㇋㇌㇍㇎㇏"
    u8"㇐㇑㇒㇓㇔㇕㇖㇗㇘㇙㇚㇛㇜㇝㇞㇟㇠㇡㇢㇣";

void random_macos_tests() {
    SCOPED_FRAME();

    // char buf[99999];
    // fgets(buf, _countof(buf), stdin);
    // ccstr strings[] = {string};

    auto basefont = CTFontCreateWithName(CFSTR("Menlo"), 12, NULL);
    Table<CTFontRef> fonts; fonts.init();

    auto load_font = [&](ccstr name) {
        bool found = false;
        auto font = fonts.get(name, &found);
        if (found) return font;

        // print("creating new font %s", name);

        auto cfname = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
        font = CTFontCreateWithName(cfname, CTFontGetSize(basefont), NULL);
        fonts.set(name, font);
        return font;
    };

    auto test_font_char = [&](CTFontRef font, uchar ch) {
        if (!font) return false;

        unichar chars[2] = {0};
        int count = CFStringGetSurrogatePairForLongCharacter(ch, chars) ? 2 : 1;

        CGGlyph glyphs[2] = {0};
        return CTFontGetGlyphsForCharacters(font, chars, glyphs, count);
    };

    auto collection = CTFontCollectionCreateFromAvailableFonts(NULL);
    defer { CFRelease(collection); };

    auto all_fonts = CTFontCollectionCreateMatchingFontDescriptors(collection);
    defer { CFRelease(all_fonts); };

    auto all_font_names = alloc_list<ccstr>(CFArrayGetCount(all_fonts));
    for (int i = 0, count = CFArrayGetCount(all_fonts); i < count; i++) {
        auto font_desc = (CTFontDescriptorRef)CFArrayGetValueAtIndex(all_fonts, i);
        auto font = CTFontCreateWithFontDescriptor(font_desc, CTFontGetSize(basefont), NULL);
        if (!font) continue;
        defer { CFRelease(font); };

        auto uncasted_name = CTFontCopyPostScriptName(font);
        defer { CFRelease(uncasted_name); };

        if (!CFStringCompare(uncasted_name, CFSTR("LastResort"), 0)) continue;

        all_font_names->append(cfstring_to_ccstr(uncasted_name));
    }

    auto find_font_for_char = [&](uchar uch) {
        if (test_font_char(basefont, uch)) return true;

        // print("base font failed for %x", uch);

        auto pat = FcPatternCreate();
        if (!pat) cp_panic("FcPatternCreate failed");
        defer { FcPatternDestroy(pat); };

        auto charset = FcCharSetCreate();
        if (!charset) cp_panic("FcCharSetCreate failed");
        defer { FcCharSetDestroy(charset); };

        if (!FcCharSetAddChar(charset, uch)) cp_panic("FcCharSetAddChar failed");
        if (!FcPatternAddCharSet(pat, FC_CHARSET, charset)) cp_panic("FcPatternAddCharSet failed");

        FcConfigSubstitute(NULL, pat, FcMatchPattern);
        FcDefaultSubstitute(pat);

        FcResult result;
        auto match = FcFontMatch(NULL, pat, &result);
        if (!match) cp_panic("FcFontMatch failed");
        defer { FcPatternDestroy(match); };

        FcChar8 *uncasted_name = NULL;
        if (FcPatternGetString(match, FC_POSTSCRIPT_NAME, 0, &uncasted_name) != FcResultMatch) {
            print("unable to get postscript name for font");
            return false;
        }

        auto name = cp_strdup((ccstr)uncasted_name);
        if (streq(name, "LastResort")) {
            bool found = false;

            For (*all_font_names) {
                if (test_font_char(load_font(it), uch)) {
                    print("fontcache couldn't find, but found through all_font_names: %s", it);
                    return true;
                }
            }
            return false;
        }

        return test_font_char(load_font(name), uch);
    };

    int fails = 0, total = 0;
    Cstr_To_Ustr conv; conv.init();
    for (int i = 0, len = strlen(big_string); i < len; i++) {
        uchar uch = 0;
        if (!conv.feed(big_string[i], &uch)) continue;
        if (uch < 255) continue;

        // if (total++ % 100 == 0)  print("%d/%d", i+1, len);
        if (!find_font_for_char(uch)) fails++;
    }
    print("failed %d/%d", fails, total);
    exit(0);
}
// 3591 fails with old, 1302 with new
