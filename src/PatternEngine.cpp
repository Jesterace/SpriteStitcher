#include "PatternEngine.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QTextStream>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

PatternEngine::PatternEngine(QObject *parent) : QObject(parent) {
    // Full built-in DMC palette used for color matching and palette editor replacement choices.
    m_dmc = {
        {"01", "White Tin", QColor(197,197,197)},
        {"02", "Tin", QColor(171,171,171)},
        {"03", "Medium Tin", QColor(144,143,144)},
        {"04", "Dark Tin", QColor(103,100,100)},
        {"05", "Light Driftwood", QColor(187,175,165)},
        {"06", "Medium Light Driftwood", QColor(180,162,147)},
        {"07", "Driftwood", QColor(116,98,86)},
        {"08", "Dark Driftwood", QColor(105,91,80)},
        {"09", "Very Dark Cocoa", QColor(72,56,55)},
        {"10", "Very Light Tender Green", QColor(206,206,184)},
        {"11", "Light Tender Green", QColor(224,220,132)},
        {"12", "Tender Green", QColor(207,198,98)},
        {"13", "Medium Light Nile Green", QColor(152,194,156)},
        {"14", "Pale Apple Green", QColor(206,209,148)},
        {"15", "Apple Green", QColor(205,214,132)},
        {"16", "Light Chartreuse", QColor(176,181,106)},
        {"17", "Light Yellow Plum", QColor(213,197,81)},
        {"18", "Yellow Plum", QColor(217,191,74)},
        {"19", "Medium Light Autumn Gold", QColor(204,156,82)},
        {"20", "Shrimp", QColor(247,209,179)},
        {"21", "Light Alizarin", QColor(192,97,77)},
        {"22", "Alizarin", QColor(146,42,41)},
        {"23", "Apple Blossom", QColor(232,218,223)},
        {"24", "White Lavender", QColor(224,217,228)},
        {"25", "Ultra Light Lavender", QColor(208,202,219)},
        {"26", "Pale Lavender", QColor(187,176,209)},
        {"27", "White Violet", QColor(217,212,216)},
        {"28", "Medium Light Eggplant", QColor(129,120,160)},
        {"29", "Eggplant", QColor(73,60,94)},
        {"30", "Medium Light Blueberry", QColor(138,137,176)},
        {"31", "Blueberry", QColor(101,98,157)},
        {"32", "Dark Blueberry", QColor(86,83,133)},
        {"33", "Fuchsia", QColor(136,75,142)},
        {"34", "Dark Fuchsia", QColor(122,48,112)},
        {"35", "Very Dark Fuchsia", QColor(88,39,75)},
        {"3713", "Salmon Very Light", QColor(255,226,226)},
        {"761", "Salmon Light", QColor(255,201,201)},
        {"760", "Salmon", QColor(245,173,173)},
        {"3712", "Salmon Medium", QColor(241,135,135)},
        {"3328", "Salmon Dark", QColor(227,109,109)},
        {"347", "Salmon Very Dark", QColor(191,45,45)},
        {"353", "Peach", QColor(254,215,204)},
        {"352", "Coral Light", QColor(253,156,151)},
        {"351", "Coral", QColor(233,106,103)},
        {"350", "Coral Medium", QColor(224,72,72)},
        {"349", "Coral Dark", QColor(210,16,53)},
        {"817", "Coral Red Very Dark", QColor(187,5,31)},
        {"3708", "Melon Light", QColor(255,203,213)},
        {"3706", "Melon Medium", QColor(255,173,188)},
        {"3705", "Melon Dark", QColor(255,121,146)},
        {"3801", "Melon Very Dark", QColor(231,73,103)},
        {"666", "Bright Red", QColor(227,29,66)},
        {"321", "Red", QColor(199,43,59)},
        {"304", "Red Medium", QColor(183,31,51)},
        {"498", "Red Dark", QColor(167,19,43)},
        {"816", "Garnet", QColor(151,11,35)},
        {"815", "Garnet Medium", QColor(135,7,31)},
        {"814", "Garnet Dark", QColor(123,0,27)},
        {"894", "Carnation Very Light", QColor(255,178,187)},
        {"893", "Carnation Light", QColor(252,144,162)},
        {"892", "Carnation Medium", QColor(255,121,140)},
        {"891", "Carnation Dark", QColor(255,87,115)},
        {"818", "Baby Pink", QColor(255,223,217)},
        {"957", "Geranium Pale", QColor(253,181,181)},
        {"956", "Geranium", QColor(255,145,145)},
        {"309", "Rose Dark", QColor(86,74,74)},
        {"963", "Dusty Rose Ult Vy Lt", QColor(255,215,215)},
        {"3716", "Dusty Rose Med Vy Lt", QColor(255,189,189)},
        {"962", "Dusty Rose Medium", QColor(230,138,138)},
        {"961", "Dusty Rose Dark", QColor(207,115,115)},
        {"3833", "Raspberry Light", QColor(234,134,153)},
        {"3832", "Raspberry Medium", QColor(219,85,110)},
        {"3831", "Raspberry Dark", QColor(179,47,72)},
        {"777", "Raspberry Very Dark", QColor(145,53,70)},
        {"819", "Baby Pink Light", QColor(255,238,235)},
        {"3326", "Rose Light", QColor(251,173,180)},
        {"776", "Pink Medium", QColor(252,176,185)},
        {"899", "Rose Medium", QColor(242,118,136)},
        {"335", "Rose", QColor(238,84,110)},
        {"326", "Rose Very Dark", QColor(179,59,75)},
        {"151", "Dusty Rose Vry Lt", QColor(240,206,212)},
        {"3354", "Dusty Rose Light", QColor(228,166,172)},
        {"3733", "Dusty Rose", QColor(232,135,155)},
        {"3731", "Dusty Rose Very Dark", QColor(218,103,131)},
        {"3350", "Dusty Rose Ultra Dark", QColor(188,67,101)},
        {"150", "Dusty Rose Ult Vy Dk", QColor(171,2,73)},
        {"3689", "Mauve Light", QColor(251,191,194)},
        {"3688", "Mauve Medium", QColor(231,169,172)},
        {"3687", "Mauve", QColor(201,107,112)},
        {"3803", "Mauve Dark", QColor(171,51,87)},
        {"3685", "Mauve Very Dark", QColor(136,21,49)},
        {"605", "Cranberry Very Light", QColor(255,192,205)},
        {"604", "Cranberry Light", QColor(255,176,190)},
        {"603", "Cranberry", QColor(255,164,190)},
        {"602", "Cranberry Medium", QColor(226,72,116)},
        {"601", "Cranberry Dark", QColor(209,40,106)},
        {"600", "Cranberry Very Dark", QColor(205,47,99)},
        {"3806", "Cyclamen Pink Light", QColor(255,140,174)},
        {"3805", "Cyclamen Pink", QColor(243,71,139)},
        {"3804", "Cyclamen Pink Dark", QColor(224,40,118)},
        {"3609", "Plum Ultra Light", QColor(244,174,213)},
        {"3608", "Plum Very Light", QColor(234,156,196)},
        {"3607", "Plum Light", QColor(197,73,137)},
        {"718", "Plum", QColor(156,36,98)},
        {"917", "Plum Medium", QColor(155,19,89)},
        {"915", "Plum Dark", QColor(130,0,67)},
        {"225", "Shell Pink Ult Vy Lt", QColor(255,223,213)},
        {"224", "Shell Pink Very Light", QColor(235,183,175)},
        {"152", "Shell Pink Med Light", QColor(226,160,153)},
        {"223", "Shell Pink Light", QColor(204,132,124)},
        {"3722", "Shell Pink Med", QColor(188,108,100)},
        {"3721", "Shell Pink Dark", QColor(161,75,81)},
        {"221", "Shell Pink Vy Dk", QColor(136,62,67)},
        {"778", "Antique Mauve Vy Lt", QColor(223,179,187)},
        {"3727", "Antique Mauve Light", QColor(219,169,178)},
        {"316", "Antique Mauve Med", QColor(183,115,127)},
        {"3726", "Antique Mauve Dark", QColor(155,91,102)},
        {"315", "Antique Mauve Md Dk", QColor(129,73,82)},
        {"3802", "Antique Mauve Vy Dk", QColor(113,65,73)},
        {"902", "Garnet Very Dark", QColor(130,38,55)},
        {"3743", "Antique Violet Vy Lt", QColor(215,203,211)},
        {"3042", "Antique Violet Light", QColor(183,157,167)},
        {"3041", "Antique Violet Medium", QColor(149,111,124)},
        {"3740", "Antique Violet Dark", QColor(120,87,98)},
        {"3836", "Grape Light", QColor(186,145,170)},
        {"3835", "Grape Medium", QColor(148,96,131)},
        {"3834", "Grape Dark", QColor(114,55,93)},
        {"154", "Grape Very Dark", QColor(87,36,51)},
        {"211", "Lavender Light", QColor(227,203,227)},
        {"210", "Lavender Medium", QColor(195,159,195)},
        {"209", "Lavender Dark", QColor(163,123,167)},
        {"208", "Lavender Very Dark", QColor(131,91,139)},
        {"3837", "Lavender Ultra Dark", QColor(108,58,110)},
        {"327", "Violet Dark", QColor(99,54,102)},
        {"153", "Violet Very Light", QColor(230,204,217)},
        {"554", "Violet Light", QColor(219,179,203)},
        {"553", "Violet", QColor(163,99,139)},
        {"552", "Violet  Medium", QColor(128,58,107)},
        {"550", "Violet Very Dark", QColor(92,24,78)},
        {"3747", "Blue Violet Vy Lt", QColor(211,215,237)},
        {"341", "Blue Violet Light", QColor(183,191,221)},
        {"156", "Blue Violet Med Lt", QColor(163,174,209)},
        {"340", "Blue Violet Medium", QColor(173,167,199)},
        {"155", "Blue Violet Med Dark", QColor(152,145,182)},
        {"3746", "Blue Violet Dark", QColor(119,107,152)},
        {"333", "Blue Violet Very Dark", QColor(92,84,120)},
        {"157", "Cornflower Blue Vy Lt", QColor(187,195,217)},
        {"794", "Cornflower Blue Light", QColor(143,156,193)},
        {"793", "Cornflower Blue Med", QColor(112,125,162)},
        {"3807", "Cornflower Blue", QColor(96,103,140)},
        {"792", "Cornflower Blue Dark", QColor(85,91,123)},
        {"158", "Cornflower Blu M V D", QColor(76,82,110)},
        {"791", "Cornflower Blue V D", QColor(70,69,99)},
        {"3840", "Lavender Blue Light", QColor(176,192,218)},
        {"3839", "Lavender Blue Med", QColor(123,142,171)},
        {"3838", "Lavender Blue Dark", QColor(92,114,148)},
        {"800", "Delft Blue Pale", QColor(192,204,222)},
        {"809", "Delft Blue", QColor(148,168,198)},
        {"799", "Delft Blue Medium", QColor(116,142,182)},
        {"798", "Delft Blue Dark", QColor(70,106,142)},
        {"797", "Royal Blue", QColor(19,71,125)},
        {"796", "Royal Blue Dark", QColor(17,65,109)},
        {"820", "Royal Blue Very Dark", QColor(14,54,92)},
        {"162", "Blue Ultra Very Light", QColor(219,236,245)},
        {"827", "Blue Very Light", QColor(189,221,237)},
        {"813", "Blue Light", QColor(161,194,215)},
        {"826", "Blue Medium", QColor(107,158,191)},
        {"825", "Blue Dark", QColor(71,129,165)},
        {"824", "Blue Very Dark", QColor(57,105,135)},
        {"996", "Electric Blue Medium", QColor(48,194,236)},
        {"3843", "Electric Blue", QColor(20,170,208)},
        {"995", "Electric Blue Dark", QColor(38,150,182)},
        {"3846", "Turquoise Bright Light", QColor(6,227,230)},
        {"3845", "Turquoise Bright Med", QColor(4,196,202)},
        {"3844", "Turquoise Bright Dark", QColor(18,174,186)},
        {"159", "Blue Gray Light", QColor(199,202,215)},
        {"160", "Blue Gray Medium", QColor(153,159,183)},
        {"161", "Blue Gray", QColor(120,128,164)},
        {"3756", "Baby Blue Ult Vy Lt", QColor(238,252,252)},
        {"775", "Baby Blue Very Light", QColor(217,235,241)},
        {"3841", "Baby Blue Pale", QColor(205,223,237)},
        {"3325", "Baby Blue Light", QColor(184,210,230)},
        {"3755", "Baby Blue", QColor(147,180,206)},
        {"334", "Baby Blue Medium", QColor(115,159,193)},
        {"322", "Baby Blue Dark", QColor(90,143,184)},
        {"312", "Baby Blue Very Dark", QColor(53,102,139)},
        {"803", "Baby Blue Ult Vy Dk", QColor(44,89,124)},
        {"336", "Navy Blue", QColor(37,59,115)},
        {"823", "Navy Blue Dark", QColor(33,48,99)},
        {"939", "Navy Blue Very Dark", QColor(27,40,83)},
        {"3753", "Antique Blue Ult Vy Lt", QColor(219,226,233)},
        {"3752", "Antique Blue Very Lt", QColor(199,209,219)},
        {"932", "Antique Blue Light", QColor(162,181,198)},
        {"931", "Antique Blue Medium", QColor(106,133,158)},
        {"930", "Antique Blue Dark", QColor(69,92,113)},
        {"3750", "Antique Blue Very Dk", QColor(56,76,94)},
        {"828", "Sky Blue Vy Lt", QColor(197,232,237)},
        {"3761", "Sky Blue Light", QColor(172,216,226)},
        {"519", "Sky Blue", QColor(126,177,200)},
        {"518", "Wedgewood Light", QColor(79,147,167)},
        {"3760", "Wedgewood Med", QColor(62,133,162)},
        {"517", "Wedgewood Dark", QColor(59,118,143)},
        {"3842", "Wedgewood Vry Dk", QColor(50,102,124)},
        {"311", "Wedgewood Ult VyDk", QColor(28,80,102)},
        {"747", "Peacock Blue Vy Lt", QColor(229,252,253)},
        {"3766", "Peacock Blue Light", QColor(153,207,217)},
        {"807", "Peacock Blue", QColor(100,171,186)},
        {"806", "Peacock Blue Dark", QColor(61,149,165)},
        {"3765", "Peacock Blue Vy Dk", QColor(52,127,140)},
        {"3811", "Turquoise Very Light", QColor(188,227,230)},
        {"598", "Turquoise Light", QColor(144,195,204)},
        {"597", "Turquoise", QColor(91,163,179)},
        {"3810", "Turquoise Dark", QColor(72,142,154)},
        {"3809", "Turquoise Vy Dark", QColor(63,124,133)},
        {"3808", "Turquoise Ult Vy Dk", QColor(54,105,112)},
        {"928", "Gray Green Vy Lt", QColor(221,227,227)},
        {"927", "Gray Green Light", QColor(189,203,203)},
        {"926", "Gray Green Med", QColor(152,174,174)},
        {"3768", "Gray Green Dark", QColor(101,127,127)},
        {"924", "Gray Green Vy Dark", QColor(86,106,106)},
        {"3849", "Teal Green Light", QColor(82,179,164)},
        {"3848", "Teal Green Med", QColor(85,147,146)},
        {"3847", "Teal Green Dark", QColor(52,125,117)},
        {"964", "Sea Green Light", QColor(169,226,216)},
        {"959", "Sea Green Med", QColor(89,199,180)},
        {"958", "Sea Green Dark", QColor(62,182,161)},
        {"3812", "Sea Green Vy Dk", QColor(47,140,132)},
        {"3851", "Green Bright Lt", QColor(73,179,161)},
        {"943", "Green Bright Md", QColor(61,147,132)},
        {"3850", "Green Bright Dk", QColor(55,132,119)},
        {"993", "Aquamarine Vy Lt", QColor(144,192,180)},
        {"992", "Aquamarine Lt", QColor(111,174,159)},
        {"3814", "Aquamarine", QColor(80,139,125)},
        {"991", "Aquamarine Dk", QColor(71,123,110)},
        {"966", "Jade Ultra Vy Lt", QColor(185,215,192)},
        {"564", "Jade Very Light", QColor(167,205,175)},
        {"563", "Jade Light", QColor(143,192,152)},
        {"562", "Jade Medium", QColor(83,151,106)},
        {"505", "Jade Green", QColor(51,131,98)},
        {"3817", "Celadon Green Lt", QColor(153,195,170)},
        {"3816", "Celadon Green", QColor(101,165,125)},
        {"163", "Celadon Green Md", QColor(77,131,97)},
        {"3815", "Celadon Green Dk", QColor(71,119,89)},
        {"561", "Celadon Green VD", QColor(44,106,69)},
        {"504", "Blue Green Vy Lt", QColor(196,222,204)},
        {"3813", "Blue Green Lt", QColor(178,212,189)},
        {"503", "Blue Green Med", QColor(123,172,148)},
        {"502", "Blue Green", QColor(91,144,113)},
        {"501", "Blue Green Dark", QColor(57,111,82)},
        {"500", "Blue Green Vy Dk", QColor(4,77,51)},
        {"955", "Nile Green Light", QColor(162,214,173)},
        {"954", "Nile Green", QColor(136,186,145)},
        {"913", "Nile Green Med", QColor(109,171,119)},
        {"912", "Emerald Green Lt", QColor(27,157,107)},
        {"911", "Emerald Green Med", QColor(24,144,101)},
        {"910", "Emerald Green Dark", QColor(24,126,86)},
        {"909", "Emerald Green Vy Dk", QColor(21,111,73)},
        {"3818", "Emerald Grn Ult V Dk", QColor(17,90,59)},
        {"369", "Pistachio Green Vy Lt", QColor(215,237,204)},
        {"368", "Pistachio Green Lt", QColor(166,194,152)},
        {"320", "Pistachio Green Med", QColor(105,136,90)},
        {"367", "Pistachio Green Dk", QColor(97,122,82)},
        {"319", "Pistachio Grn Vy Dk", QColor(32,95,46)},
        {"890", "Pistachio Grn Ult V D", QColor(23,73,35)},
        {"164", "Forest Green Lt", QColor(200,216,184)},
        {"989", "Forest Green", QColor(141,166,117)},
        {"988", "Forest Green Med", QColor(115,139,91)},
        {"987", "Forest Green Dk", QColor(88,113,65)},
        {"986", "Forest Green Vy Dk", QColor(64,82,48)},
        {"772", "Yellow Green Vy Lt", QColor(228,236,212)},
        {"3348", "Yellow Green Lt", QColor(204,217,177)},
        {"3347", "Yellow Green Med", QColor(113,147,92)},
        {"3346", "Hunter Green", QColor(64,106,58)},
        {"3345", "Hunter Green Dk", QColor(27,89,21)},
        {"895", "Hunter Green Vy Dk", QColor(27,83,0)},
        {"704", "Chartreuse Bright", QColor(158,207,52)},
        {"703", "Chartreuse", QColor(123,181,71)},
        {"702", "Kelly Green", QColor(71,167,47)},
        {"701", "Green Light", QColor(63,143,41)},
        {"700", "Green Bright", QColor(7,115,27)},
        {"699", "Green", QColor(5,101,23)},
        {"907", "Parrot Green Lt", QColor(199,230,102)},
        {"906", "Parrot Green Md", QColor(127,179,53)},
        {"905", "Parrot Green Dk", QColor(98,138,40)},
        {"904", "Parrot Green V Dk", QColor(85,120,34)},
        {"472", "Avocado Grn U Lt", QColor(216,228,152)},
        {"471", "Avocado Grn V Lt", QColor(174,191,121)},
        {"470", "Avocado Grn Lt", QColor(148,171,79)},
        {"469", "Avocado Green", QColor(114,132,60)},
        {"937", "Avocado Green Md", QColor(98,113,51)},
        {"936", "Avocado Grn V Dk", QColor(76,88,38)},
        {"935", "Avocado Green Dk", QColor(66,77,33)},
        {"934", "Avocado Grn Black", QColor(49,57,25)},
        {"523", "Fern Green Lt", QColor(171,177,151)},
        {"3053", "Green Gray", QColor(156,164,130)},
        {"3052", "Green Gray Md", QColor(136,146,104)},
        {"3051", "Green Gray Dk", QColor(95,102,72)},
        {"524", "Fern Green Vy Lt", QColor(196,205,172)},
        {"522", "Fern Green", QColor(150,158,126)},
        {"520", "Fern Green Dark", QColor(102,109,79)},
        {"3364", "Pine Green", QColor(131,151,95)},
        {"3363", "Pine Green Md", QColor(114,130,86)},
        {"3362", "Pine Green Dk", QColor(94,107,71)},
        {"165", "Moss Green Vy Lt", QColor(239,244,164)},
        {"3819", "Moss Green Lt", QColor(224,232,104)},
        {"166", "Moss Green Md Lt", QColor(192,200,64)},
        {"581", "Moss Green", QColor(167,174,56)},
        {"580", "Moss Green Dk", QColor(136,141,51)},
        {"734", "Olive Green Lt", QColor(199,192,119)},
        {"733", "Olive Green Md", QColor(188,179,76)},
        {"732", "Olive Green", QColor(148,140,54)},
        {"731", "Olive Green Dk", QColor(147,139,55)},
        {"730", "Olive Green V Dk", QColor(130,123,48)},
        {"3013", "Khaki Green Lt", QColor(185,185,130)},
        {"3012", "Khaki Green Md", QColor(166,167,93)},
        {"3011", "Khaki Green Dk", QColor(137,138,88)},
        {"372", "Mustard Lt", QColor(204,183,132)},
        {"371", "Mustard", QColor(191,166,113)},
        {"370", "Mustard Medium", QColor(184,157,100)},
        {"834", "Golden Olive Vy Lt", QColor(219,190,127)},
        {"833", "Golden Olive Lt", QColor(200,171,108)},
        {"832", "Golden Olive", QColor(189,155,81)},
        {"831", "Golden Olive Md", QColor(170,143,86)},
        {"830", "Golden Olive Dk", QColor(141,120,75)},
        {"829", "Golden Olive Vy Dk", QColor(126,107,66)},
        {"613", "Drab Brown V Lt", QColor(220,196,170)},
        {"612", "Drab Brown Lt", QColor(188,154,120)},
        {"611", "Drab Brown", QColor(150,118,86)},
        {"610", "Drab Brown Dk", QColor(121,96,71)},
        {"3047", "Yellow Beige Lt", QColor(231,214,193)},
        {"3046", "Yellow Beige Md", QColor(216,188,154)},
        {"3045", "Yellow Beige Dk", QColor(188,150,106)},
        {"167", "Yellow Beige V Dk", QColor(167,124,73)},
        {"746", "Off White", QColor(252,252,238)},
        {"677", "Old Gold Vy Lt", QColor(245,236,203)},
        {"422", "Hazelnut Brown Lt", QColor(198,159,123)},
        {"3828", "Hazelnut Brown", QColor(183,139,97)},
        {"420", "Hazelnut Brown Dk", QColor(160,112,66)},
        {"869", "Hazelnut Brown V Dk", QColor(131,94,57)},
        {"728", "Topaz", QColor(228,180,104)},
        {"783", "Topaz Medium", QColor(206,145,36)},
        {"782", "Topaz Dark", QColor(174,119,32)},
        {"781", "Topaz Very Dark", QColor(162,109,32)},
        {"780", "Topaz Ultra Vy Dk", QColor(148,99,26)},
        {"676", "Old Gold Lt", QColor(229,206,151)},
        {"729", "Old Gold Medium", QColor(208,165,62)},
        {"680", "Old Gold Dark", QColor(188,141,14)},
        {"3829", "Old Gold Vy Dark", QColor(169,130,4)},
        {"3822", "Straw Light", QColor(246,220,152)},
        {"3821", "Straw", QColor(243,206,117)},
        {"3820", "Straw Dark", QColor(223,182,95)},
        {"3852", "Straw Very Dark", QColor(205,157,55)},
        {"445", "Lemon Light", QColor(255,251,139)},
        {"307", "Lemon", QColor(253,237,84)},
        {"973", "Canary Bright", QColor(255,227,0)},
        {"444", "Lemon Dark", QColor(255,214,0)},
        {"3078", "Golden Yellow Vy Lt", QColor(253,249,205)},
        {"727", "Topaz Vy Lt", QColor(255,241,175)},
        {"726", "Topaz Light", QColor(253,215,85)},
        {"725", "Topaz Med Lt", QColor(255,200,64)},
        {"972", "Canary Deep", QColor(255,181,21)},
        {"745", "Yellow Pale Light", QColor(255,233,173)},
        {"744", "Yellow Pale", QColor(255,231,147)},
        {"743", "Yellow Med", QColor(254,211,118)},
        {"742", "Tangerine Light", QColor(255,191,87)},
        {"741", "Tangerine Med", QColor(255,163,43)},
        {"740", "Tangerine", QColor(255,139,0)},
        {"970", "Pumpkin Light", QColor(247,139,19)},
        {"971", "Pumpkin", QColor(246,127,0)},
        {"947", "Burnt Orange", QColor(255,123,77)},
        {"946", "Burnt Orange Med", QColor(235,99,7)},
        {"900", "Burnt Orange Dark", QColor(209,88,7)},
        {"967", "Apricot Very Light", QColor(255,222,213)},
        {"3824", "Apricot Light", QColor(254,205,194)},
        {"3341", "Apricot", QColor(252,171,152)},
        {"3340", "Apricot Med", QColor(255,131,111)},
        {"608", "Burnt Orange Bright", QColor(253,93,53)},
        {"606", "Orange?Red Bright", QColor(250,50,3)},
        {"951", "Tawny Light", QColor(255,226,207)},
        {"3856", "Mahogany Ult Vy Lt", QColor(255,211,181)},
        {"722", "Orange Spice Light", QColor(247,151,111)},
        {"721", "Orange Spice Med", QColor(242,120,66)},
        {"720", "Orange Spice Dark", QColor(229,92,31)},
        {"3825", "Pumpkin Pale", QColor(253,189,150)},
        {"922", "Copper Light", QColor(226,115,35)},
        {"921", "Copper", QColor(198,98,24)},
        {"920", "Copper Med", QColor(172,84,20)},
        {"919", "Red?Copper", QColor(166,69,16)},
        {"918", "Red?Copper Dark", QColor(130,52,10)},
        {"3770", "Tawny Vy Light", QColor(255,238,227)},
        {"945", "Tawny", QColor(251,213,187)},
        {"402", "Mahogany Vy Lt", QColor(247,167,119)},
        {"3776", "Mahogany Light", QColor(207,121,57)},
        {"301", "Mahogany Med", QColor(179,95,43)},
        {"400", "Mahogany Dark", QColor(143,67,15)},
        {"300", "Mahogany Vy Dk", QColor(111,47,0)},
        {"3823", "Yellow Ultra Pale", QColor(255,253,227)},
        {"3855", "Autumn Gold Lt", QColor(250,211,150)},
        {"3854", "Autumn Gold Med", QColor(242,175,104)},
        {"3853", "Autumn Gold Dk", QColor(242,151,70)},
        {"3827", "Golden Brown Pale", QColor(247,187,119)},
        {"977", "Golden Brown Light", QColor(220,156,86)},
        {"976", "Golden Brown Med", QColor(194,129,66)},
        {"3826", "Golden Brown", QColor(173,114,57)},
        {"975", "Golden Brown Dk", QColor(145,79,18)},
        {"948", "Peach Very Light", QColor(254,231,218)},
        {"754", "Peach Light", QColor(247,203,191)},
        {"3771", "Terra Cotta Ult Vy Lt", QColor(244,187,169)},
        {"758", "Terra Cotta Vy Lt", QColor(238,170,155)},
        {"3778", "Terra Cotta Light", QColor(217,137,120)},
        {"356", "Terra Cotta Med", QColor(197,106,91)},
        {"3830", "Terra Cotta", QColor(185,85,68)},
        {"355", "Terra Cotta Dark", QColor(152,68,54)},
        {"3777", "Terra Cotta Vy Dk", QColor(134,48,34)},
        {"3779", "Rosewood Ult Vy Lt", QColor(248,202,200)},
        {"3859", "Rosewood Light", QColor(186,139,124)},
        {"3858", "Rosewood Med", QColor(150,74,63)},
        {"3857", "Rosewood Dark", QColor(104,37,26)},
        {"3774", "Desert Sand Vy Lt", QColor(243,225,215)},
        {"950", "Desert Sand Light", QColor(238,211,196)},
        {"3064", "Desert Sand", QColor(196,142,112)},
        {"407", "Desert Sand Med", QColor(187,129,97)},
        {"3773", "Desert Sand Dark", QColor(182,117,82)},
        {"3772", "Desert Sand Vy Dk", QColor(160,108,80)},
        {"632", "Desert Sand Ult Vy Dk", QColor(135,85,57)},
        {"453", "Shell Gray Light", QColor(215,206,203)},
        {"452", "Shell Gray Med", QColor(192,179,174)},
        {"451", "Shell Gray Dark", QColor(145,123,115)},
        {"3861", "Cocoa Light", QColor(166,136,129)},
        {"3860", "Cocoa", QColor(125,93,87)},
        {"779", "Cocoa Dark", QColor(98,75,69)},
        {"712", "Cream", QColor(255,251,239)},
        {"739", "Tan Ult Vy Lt", QColor(248,228,200)},
        {"738", "Tan Very Light", QColor(236,204,158)},
        {"437", "Tan Light", QColor(228,187,142)},
        {"436", "Tan", QColor(203,144,81)},
        {"435", "Brown Very Light", QColor(184,119,72)},
        {"434", "Brown Light", QColor(152,94,51)},
        {"433", "Brown Med", QColor(122,69,31)},
        {"801", "Coffee Brown Dk", QColor(101,57,25)},
        {"898", "Coffee Brown Vy Dk", QColor(73,42,19)},
        {"938", "Coffee Brown Ult Dk", QColor(54,31,14)},
        {"3371", "Black Brown", QColor(30,17,8)},
        {"543", "Beige Brown Ult Vy Lt", QColor(242,227,206)},
        {"3864", "Mocha Beige Light", QColor(203,182,156)},
        {"3863", "Mocha Beige Med", QColor(164,131,92)},
        {"3862", "Mocha Beige Dark", QColor(138,110,78)},
        {"3031", "Mocha Brown Vy Dk", QColor(75,60,42)},
        {"B5200", "Snow White", QColor(255,255,255)},
        {"White", "White", QColor(252,251,248)},
        {"3865", "Winter White", QColor(249,247,241)},
        {"Ecru", "Ecru", QColor(240,234,218)},
        {"822", "Beige Gray Light", QColor(231,226,211)},
        {"644", "Beige Gray Med", QColor(221,216,203)},
        {"642", "Beige Gray Dark", QColor(164,152,120)},
        {"640", "Beige Gray Vy Dk", QColor(133,123,97)},
        {"3787", "Brown Gray Dark", QColor(98,93,80)},
        {"3021", "Brown Gray Vy Dk", QColor(79,75,65)},
        {"3024", "Brown Gray Vy Lt", QColor(235,234,231)},
        {"3023", "Brown Gray Light", QColor(177,170,151)},
        {"3022", "Brown Gray Med", QColor(142,144,120)},
        {"535", "Ash Gray Vy Lt", QColor(99,100,88)},
        {"3033", "Mocha Brown Vy Lt", QColor(227,216,204)},
        {"3782", "Mocha Brown Lt", QColor(210,188,166)},
        {"3032", "Mocha Brown Med", QColor(179,159,139)},
        {"3790", "Beige Gray Ult Dk", QColor(127,106,85)},
        {"3781", "Mocha Brown Dk", QColor(107,87,67)},
        {"3866", "Mocha Brn Ult Vy Lt", QColor(250,246,240)},
        {"842", "Beige Brown Vy Lt", QColor(209,186,161)},
        {"841", "Beige Brown Lt", QColor(182,155,126)},
        {"840", "Beige Brown Med", QColor(154,124,92)},
        {"839", "Beige Brown Dk", QColor(103,85,65)},
        {"838", "Beige Brown Vy Dk", QColor(89,73,55)},
        {"3072", "Beaver Gray Vy Lt", QColor(230,232,232)},
        {"648", "Beaver Gray Lt", QColor(188,180,172)},
        {"647", "Beaver Gray Med", QColor(176,166,156)},
        {"646", "Beaver Gray Dk", QColor(135,125,115)},
        {"645", "Beaver Gray Vy Dk", QColor(110,101,92)},
        {"844", "Beaver Gray Ult Dk", QColor(72,72,72)},
        {"762", "Pearl Gray Vy Lt", QColor(236,236,236)},
        {"415", "Pearl Gray", QColor(211,211,214)},
        {"318", "Steel Gray Lt", QColor(171,171,171)},
        {"414", "Steel Gray Dk", QColor(140,140,140)},
        {"168", "Pewter Very Light", QColor(209,209,209)},
        {"169", "Pewter Light", QColor(132,132,132)},
        {"317", "Pewter Gray", QColor(108,108,108)},
        {"413", "Pewter Gray Dark", QColor(86,86,86)},
        {"3799", "Pewter Gray Vy Dk", QColor(66,66,66)},
        {"310", "Black", QColor(0,0,0)}
    };
}

QString PatternEngine::safeFileBase(QString text) {
    text = text.trimmed();
    if (text.isEmpty()) text = "cross_stitch_pattern";
    text.replace(QRegularExpression("[^A-Za-z0-9._-]+"), "_");
    text.replace(QRegularExpression("_+"), "_");
    text = text.left(80);
    return text.trimmed().isEmpty() ? QStringLiteral("cross_stitch_pattern") : text;
}

QString PatternEngine::makeSymbol(int index, PatternOptions::SymbolStyle style) {
    static const QStringList classicSymbols = {
        "1","2","3","4","5","6","7","8","9",
        "A","B","C","D","E","F","G","H","J","K","L","M","N","P","Q","R","S","T","U","V","W","X","Y","Z",
        "●","○","◆","◇","■","□","▲","△","✚","✖","★","☆","♣","♠","♥","♦","x","/","\\","=","%","#","@"
    };

    static const QStringList cleanSymbols = {
        "□","○","●","◆","◇","■","▲","△","×","★","☆","♥","♦","♣","♠",
        "⌂","✿","☾","✚","◐","◑","◒","◓","◊","⊙","⊕","⊗","⊞","⊠","◎","◈",
        "A","B","C","D","E","F","G","H","J","K","L","M","N","P","Q","R","S","T","U","V","W","X","Y","Z"
    };

    static const QStringList simpleIconSymbols = {
        "□","○","⌂","✿","☾","★","♥","♦","♣","♠","▲","△","◆","◇","■",
        "◐","◑","◒","◓","◎","◈","⊕","⊗","✚","✱","/","\\","=","%","#","@",
        "A","B","C","D","E","F","G","H","J","K","L","M","N","P","Q","R","S","T","U","V","W","X","Y","Z"
    };

    const QStringList *symbols = &cleanSymbols;
    switch (style) {
        case PatternOptions::SymbolStyle::Classic:
            symbols = &classicSymbols;
            break;
        case PatternOptions::SymbolStyle::SimpleIcons:
            symbols = &simpleIconSymbols;
            break;
        case PatternOptions::SymbolStyle::Clean:
        default:
            symbols = &cleanSymbols;
            break;
    }

    static const QStringList badSymbols = {
        QStringLiteral("+"),
        QStringLiteral("⌂")
    };

    int usableIndex = 0;
    for (const QString &symbol : *symbols) {
        if (badSymbols.contains(symbol)) continue;

        if (usableIndex == index) {
            return symbol;
        }

        ++usableIndex;
    }

    return QString("C%1").arg(index + 1, 2, 10, QChar('0'));
}

int PatternEngine::colorDistanceSquared(const QColor &a, const QColor &b) {
    const int dr = a.red() - b.red();
    const int dg = a.green() - b.green();
    const int db = a.blue() - b.blue();
    return dr * dr + dg * dg + db * db;
}

QColor PatternEngine::inferBackgroundColor(const QImage &img) const {
    struct Count { QColor color; int count = 0; };
    QMap<QRgb, int> counts;

    auto add = [&](int x, int y) {
        QColor c = QColor::fromRgba(img.pixel(x, y));
        if (c.alpha() < 128) return;
        QColor rounded((c.red()/8)*8, (c.green()/8)*8, (c.blue()/8)*8);
        counts[rounded.rgb()] += 1;
    };

    for (int x = 0; x < img.width(); ++x) {
        add(x, 0);
        add(x, img.height() - 1);
    }
    for (int y = 0; y < img.height(); ++y) {
        add(0, y);
        add(img.width() - 1, y);
    }

    int best = -1;
    QColor bestColor(0, 255, 0);
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        if (it.value() > best) {
            best = it.value();
            bestColor = QColor::fromRgb(it.key());
        }
    }
    return bestColor;
}

const DmcColor &PatternEngine::nearestDmc(const QColor &color) const {
    int bestIndex = 0;
    int bestDistance = std::numeric_limits<int>::max();
    for (int i = 0; i < m_dmc.size(); ++i) {
        int d = colorDistanceSquared(color, m_dmc[i].color);
        if (d < bestDistance) {
            bestDistance = d;
            bestIndex = i;
        }
    }
    return m_dmc[bestIndex];
}


const DmcColor *PatternEngine::findDmcByCode(const QString &code) const {
    for (const auto &dmc : m_dmc) {
        if (dmc.code.compare(code.trimmed(), Qt::CaseInsensitive) == 0) return &dmc;
    }
    return nullptr;
}

QVector<DmcColor> PatternEngine::dmcPalette() const {
    return m_dmc;
}

bool PatternEngine::shouldSkipPixel(const QColor &pixel, const QColor &inferredBackground, const PatternOptions &options) const {
    if (options.transparencyAsBackground && pixel.alpha() < 128) return true;
    if (options.backgroundMode == PatternOptions::BackgroundMode::StitchEverything) return false;

    QColor target = options.backgroundMode == PatternOptions::BackgroundMode::ExactColor
                    ? options.backgroundColor
                    : inferredBackground;

    const int tolerance = options.backgroundMode == PatternOptions::BackgroundMode::ExactColor ? 0 : 7;
    return std::abs(pixel.red() - target.red()) <= tolerance &&
           std::abs(pixel.green() - target.green()) <= tolerance &&
           std::abs(pixel.blue() - target.blue()) <= tolerance;
}

void PatternEngine::buildPattern(const QImage &img,
                                 const PatternOptions &options,
                                 QVector<QVector<StitchCell>> &grid,
                                 QVector<PatternColor> &colors,
                                 int &stitched,
                                 int &unstitched,
                                 int *colorCountBeforeCleanup) const {
    const QColor inferredBackground = inferBackgroundColor(img);
    QMap<QString, int> keyToIndex;

    grid.resize(img.height());
    stitched = 0;
    unstitched = 0;

    for (int y = 0; y < img.height(); ++y) {
        grid[y].resize(img.width());
        for (int x = 0; x < img.width(); ++x) {
            QColor pixel = QColor::fromRgba(img.pixel(x, y));
            if (shouldSkipPixel(pixel, inferredBackground, options)) {
                grid[y][x].colorIndex = -1;
                ++unstitched;
                continue;
            }

            PatternColor pc;
            if (options.matchDmc) {
                const DmcColor &dmc = nearestDmc(pixel);
                pc.key = dmc.code;
                pc.dmcCode = dmc.code;
                pc.dmcName = dmc.name;
                pc.color = dmc.color;
            } else {
                pc.key = pixel.name(QColor::HexRgb).toUpper();
                pc.dmcCode = pc.key;
                pc.dmcName = "Exact RGB";
                pc.color = QColor(pixel.red(), pixel.green(), pixel.blue());
            }

            int index = keyToIndex.value(pc.key, -1);
            if (index < 0) {
                index = colors.size();
                keyToIndex[pc.key] = index;
                colors.push_back(pc);
            }
            colors[index].stitches += 1;
            grid[y][x].colorIndex = index;
            ++stitched;
        }
    }

    if (colorCountBeforeCleanup) *colorCountBeforeCleanup = colors.size();

    auto remapGrid = [&](const QVector<int> &oldToNew) {
        for (auto &row : grid) {
            for (auto &cell : row) {
                if (cell.colorIndex >= 0 && cell.colorIndex < oldToNew.size()) {
                    cell.colorIndex = oldToNew[cell.colorIndex];
                }
            }
        }
    };

    auto sortByUseThenCode = [&](QVector<int> &order) {
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            if (colors[a].stitches != colors[b].stitches) return colors[a].stitches > colors[b].stitches;
            const int codeCompare = QString::compare(colors[a].dmcCode, colors[b].dmcCode, Qt::CaseInsensitive);
            if (codeCompare != 0) return codeCompare < 0;
            return colors[a].key < colors[b].key;
        });
    };

    if (!colors.isEmpty() && options.colorCleanupMode == PatternOptions::ColorCleanupMode::MergeSimilarColors) {
        const int tolerance = std::max(0, options.mergeTolerance);
        const int thresholdSquared = tolerance * tolerance * 3;
        QVector<int> order(colors.size());
        for (int i = 0; i < order.size(); ++i) order[i] = i;
        sortByUseThenCode(order);

        QVector<int> oldToNew(colors.size(), -1);
        QVector<PatternColor> merged;
        for (int oldIndex : order) {
            int bestCluster = -1;
            int bestDistance = std::numeric_limits<int>::max();
            for (int cluster = 0; cluster < merged.size(); ++cluster) {
                const int d = colorDistanceSquared(colors[oldIndex].color, merged[cluster].color);
                if (d <= thresholdSquared && d < bestDistance) {
                    bestDistance = d;
                    bestCluster = cluster;
                }
            }

            if (bestCluster < 0) {
                bestCluster = merged.size();
                PatternColor pc = colors[oldIndex];
                pc.stitches = 0;
                merged.push_back(pc);
            }
            oldToNew[oldIndex] = bestCluster;
            merged[bestCluster].stitches += colors[oldIndex].stitches;
        }

        remapGrid(oldToNew);
        colors = merged;
    } else if (!colors.isEmpty() && options.colorCleanupMode == PatternOptions::ColorCleanupMode::LimitMaxColors) {
        const int limit = std::max(1, options.maxColors);
        if (colors.size() > limit) {
            QVector<int> order(colors.size());
            for (int i = 0; i < order.size(); ++i) order[i] = i;
            sortByUseThenCode(order);

            QVector<PatternColor> limited;
            limited.reserve(limit);
            QVector<int> selectedOld;
            selectedOld.reserve(limit);
            for (int i = 0; i < limit && i < order.size(); ++i) {
                PatternColor pc = colors[order[i]];
                pc.stitches = 0;
                limited.push_back(pc);
                selectedOld.push_back(order[i]);
            }

            QVector<int> oldToNew(colors.size(), 0);
            for (int oldIndex = 0; oldIndex < colors.size(); ++oldIndex) {
                int bestCluster = 0;
                int bestDistance = std::numeric_limits<int>::max();
                for (int cluster = 0; cluster < limited.size(); ++cluster) {
                    const int d = colorDistanceSquared(colors[oldIndex].color, limited[cluster].color);
                    if (d < bestDistance) {
                        bestDistance = d;
                        bestCluster = cluster;
                    }
                }
                oldToNew[oldIndex] = bestCluster;
                limited[bestCluster].stitches += colors[oldIndex].stitches;
            }

            remapGrid(oldToNew);
            colors = limited;
        }
    }

    if (!colors.isEmpty() && (!options.dmcOverrides.isEmpty() || !options.backgroundColorKeys.isEmpty() || options.removeTinyStitchesMax > 0)) {
        const QChar keySeparator(0x001F);
        QVector<int> oldToNew(colors.size(), -1);
        QMap<QString, int> mergeKeyToNew;
        QVector<PatternColor> edited;

        for (int oldIndex = 0; oldIndex < colors.size(); ++oldIndex) {
            const PatternColor &original = colors[oldIndex];
            const QStringList sourceKeys = original.key.split(keySeparator, Qt::SkipEmptyParts);

            bool makeBackground = false;
            for (const QString &sourceKey : sourceKeys) {
                if (options.backgroundColorKeys.contains(sourceKey)) {
                    makeBackground = true;
                    break;
                }
            }
            if (!makeBackground && options.removeTinyStitchesMax > 0 && original.stitches <= options.removeTinyStitchesMax) {
                makeBackground = true;
            }
            if (makeBackground) {
                oldToNew[oldIndex] = -1;
                continue;
            }

            PatternColor pc = original;
            QString overrideCode;
            for (const QString &sourceKey : sourceKeys) {
                if (options.dmcOverrides.contains(sourceKey)) {
                    overrideCode = options.dmcOverrides.value(sourceKey).trimmed();
                    break;
                }
            }

            QString mergeKey;
            if (!overrideCode.isEmpty()) {
                if (const DmcColor *dmc = findDmcByCode(overrideCode)) {
                    pc.dmcCode = dmc->code;
                    pc.dmcName = dmc->name;
                    pc.color = dmc->color;
                    mergeKey = QStringLiteral("DMC|") + dmc->code.toUpper();
                } else {
                    mergeKey = QStringLiteral("KEY|") + pc.key;
                }
            } else {
                mergeKey = QStringLiteral("KEY|") + pc.key;
            }

            int newIndex = mergeKeyToNew.value(mergeKey, -1);
            if (newIndex < 0) {
                newIndex = edited.size();
                mergeKeyToNew[mergeKey] = newIndex;
                pc.stitches = 0;
                edited.push_back(pc);
            } else {
                QStringList existingKeys = edited[newIndex].key.split(keySeparator, Qt::SkipEmptyParts);
                for (const QString &sourceKey : sourceKeys) {
                    if (!existingKeys.contains(sourceKey)) existingKeys << sourceKey;
                }
                edited[newIndex].key = existingKeys.join(QString(keySeparator));
            }
            oldToNew[oldIndex] = newIndex;
        }

        stitched = 0;
        unstitched = 0;
        for (auto &row : grid) {
            for (auto &cell : row) {
                if (cell.colorIndex < 0) {
                    ++unstitched;
                    continue;
                }
                const int oldIndex = cell.colorIndex;
                const int newIndex = (oldIndex >= 0 && oldIndex < oldToNew.size()) ? oldToNew[oldIndex] : -1;
                if (newIndex < 0) {
                    cell.colorIndex = -1;
                    ++unstitched;
                } else {
                    cell.colorIndex = newIndex;
                    edited[newIndex].stitches += 1;
                    ++stitched;
                }
            }
        }

        colors = edited;
    }


    QVector<int> order(colors.size());
    for (int i = 0; i < order.size(); ++i) order[i] = i;

    // Keep the legend predictable: numeric DMC codes in ascending numerical order.
    // Non-numeric DMC entries such as B5200/White and exact RGB fallback values come after.
    auto dmcBucket = [](const QString &code) {
        bool numeric = false;
        code.toInt(&numeric);
        if (numeric) return 0;
        if (code.compare(QStringLiteral("B5200"), Qt::CaseInsensitive) == 0 ||
            code.compare(QStringLiteral("White"), Qt::CaseInsensitive) == 0) return 1;
        return 2;
    };
    auto dmcNumber = [](const QString &code) {
        bool numeric = false;
        int value = code.toInt(&numeric);
        if (numeric) return value;
        if (code.compare(QStringLiteral("B5200"), Qt::CaseInsensitive) == 0) return 5200;
        if (code.compare(QStringLiteral("White"), Qt::CaseInsensitive) == 0) return 5201;
        return std::numeric_limits<int>::max();
    };

    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const int bucketA = dmcBucket(colors[a].dmcCode);
        const int bucketB = dmcBucket(colors[b].dmcCode);
        if (bucketA != bucketB) return bucketA < bucketB;

        const int numberA = dmcNumber(colors[a].dmcCode);
        const int numberB = dmcNumber(colors[b].dmcCode);
        if (numberA != numberB) return numberA < numberB;

        const int codeCompare = QString::compare(colors[a].dmcCode, colors[b].dmcCode, Qt::CaseInsensitive);
        if (codeCompare != 0) return codeCompare < 0;
        return colors[a].key < colors[b].key;
    });

    QVector<int> remap(colors.size());
    QVector<PatternColor> sorted;
    sorted.reserve(colors.size());
    for (int newIndex = 0; newIndex < order.size(); ++newIndex) {
        PatternColor pc = colors[order[newIndex]];
        pc.symbol = makeSymbol(newIndex, options.symbolStyle);
        remap[order[newIndex]] = newIndex;
        sorted.push_back(pc);
    }

    for (auto &row : grid) {
        for (auto &cell : row) {
            if (cell.colorIndex >= 0) cell.colorIndex = remap[cell.colorIndex];
        }
    }
    colors = sorted;
}

static QString uniqueFilePath(const QDir &dir, const QString &stem, const QString &extension) {
    QString candidate = dir.filePath(stem + extension);
    if (!QFileInfo::exists(candidate)) return candidate;

    for (int i = 2; i < 1000; ++i) {
        candidate = dir.filePath(QStringLiteral("%1_%2%3").arg(stem).arg(i).arg(extension));
        if (!QFileInfo::exists(candidate)) return candidate;
    }

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    return dir.filePath(QStringLiteral("%1_%2%3").arg(stem, stamp, extension));
}

static QString csvEscape(const QString &value) {
    QString out = value;
    out.replace('"', "\"\"");
    return '"' + out + '"';
}


static const QString kPdfDisclaimer = QStringLiteral(
    "Fan-made pattern for personal use only. Not official, sponsored, endorsed, or affiliated with Nintendo, Sega, Capcom, The Pokemon Company, or any other rights holder. Characters, sprites, logos, and trademarks remain property of their respective owners."
);

static void drawPdfDisclaimer(QPainter &p, int margin, int pageW, int pageH) {
    QFont disclaimerFont = p.font();
    disclaimerFont.setPointSize(6);
    disclaimerFont.setBold(false);
    p.setFont(disclaimerFont);
    p.setPen(QColor(90, 90, 90));

    p.drawText(QRect(margin, pageH - margin - 86, pageW - margin * 2, 78),
               Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
               kPdfDisclaimer);
}


bool PatternEngine::writeLegendCsv(const QString &csvPath,
                                   const QVector<PatternColor> &colors,
                                   QString *error) const {
    QFile f(csvPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = f.errorString();
        return false;
    }
    QTextStream ts(&f);
    ts << "symbol,dmc_code,description,stitches,rgb\n";
    for (const auto &c : colors) {
        ts << csvEscape(c.symbol) << ','
           << csvEscape(c.dmcCode) << ','
           << csvEscape(c.dmcName) << ','
           << c.stitches << ','
           << csvEscape(c.color.name(QColor::HexRgb).toUpper()) << '\n';
    }
    return true;
}


bool PatternEngine::renderPreviewPng(const QString &pngPath,
                                     const QVector<QVector<StitchCell>> &grid,
                                     const QVector<PatternColor> &colors,
                                     QString *error) const {
    if (grid.isEmpty() || grid.first().isEmpty()) {
        if (error) *error = QStringLiteral("Pattern grid is empty.");
        return false;
    }

    const int rows = grid.size();
    const int cols = grid.first().size();
    int scale = 16;
    const int maxDim = 4096;
    const int largest = std::max(cols, rows);

    if (largest * scale > maxDim) {
        scale = std::max(1, maxDim / std::max(1, largest));
    }

    QImage canvas(std::max(1, cols * scale), std::max(1, rows * scale), QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const int colorIndex = grid[y][x].colorIndex;
            if (colorIndex < 0 || colorIndex >= colors.size()) continue;

            QColor color = colors[colorIndex].color;
            color.setAlpha(255);
            painter.fillRect(QRect(x * scale, y * scale, scale, scale), color);
        }
    }

    painter.end();

    if (!canvas.save(pngPath, "PNG")) {
        if (error) *error = QStringLiteral("Could not save PNG file.");
        return false;
    }

    return true;
}


static QString makePatternKeeperImportSymbol(int index) {
    // Avoid numeric symbols in Pattern Keeper import PDFs.
    // Pattern Keeper can confuse chart symbols like "3" with DMC/thread numbers.
    static const QStringList symbols = {
        "L","Z","U","H","V","A","M","N","T","Y","R","S","K","P","Q","W","X","J",
        "(","w","<","}","?","r","c","!","-","=","/","\\","*","#","@","%",
        "■","▲","◆","●","○","♥","↑","↓","◇","□","△","★"
    };
    if (index < symbols.size()) return symbols[index];
    return QString("S%1").arg(index + 1);
}

bool PatternEngine::renderPatternKeeperImportPdf(const QString &pdfPath,
                                                 const QString &title,
                                                 const QVector<QVector<StitchCell>> &grid,
                                                 const QVector<PatternColor> &colors,
                                                 int stitched,
                                                 int unstitched,
                                                 const PatternOptions &options,
                                                 QString *error) const {
    const int rows = grid.size();
    const int cols = rows > 0 ? grid[0].size() : 0;

    QPageLayout::Orientation orientation = QPageLayout::Portrait;
    if (cols > rows + 20) orientation = QPageLayout::Landscape;

    QPdfWriter pdf(pdfPath);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setPageOrientation(orientation);
    pdf.setResolution(300);
    pdf.setCreator("SpriteStitcher v" APP_VERSION);
    pdf.setTitle(title + QStringLiteral(" Pattern Keeper Import"));
    pdf.setPageMargins(QMarginsF(0.30, 0.30, 0.30, 0.30), QPageLayout::Inch);

    QPainter p(&pdf);
    if (!p.isActive()) {
        if (error) *error = "Could not start Pattern Keeper import PDF painter.";
        return false;
    }

    QVector<PatternColor> pkColors = colors;
    for (int i = 0; i < pkColors.size(); ++i) {
        pkColors[i].symbol = makePatternKeeperImportSymbol(i);
    }

    const int pageW = pdf.width();
    const int pageH = pdf.height();
    const int margin = 75;
    const int headerH = 110;
    const int footerH = 155;

    auto codeForKey = [](const PatternColor &c) {
        if (c.dmcCode.compare(QStringLiteral("White"), Qt::CaseInsensitive) == 0 ||
            c.dmcCode.compare(QStringLiteral("BLANC"), Qt::CaseInsensitive) == 0) {
            return QStringLiteral("BLANC");
        }
        return c.dmcCode;
    };

    auto drawChartPage = [&](int pageNumber) {
        p.fillRect(QRect(0, 0, pageW, pageH), Qt::white);

        QFont titleFont = p.font();
        titleFont.setPointSize(15);
        titleFont.setBold(false);
        p.setFont(titleFont);
        p.setPen(Qt::black);
        p.drawText(QRect(margin, margin - 10, pageW - margin * 2, 35),
                   Qt::AlignCenter, title);

        QFont smallFont = p.font();
        smallFont.setPointSize(8);
        smallFont.setBold(false);
        p.setFont(smallFont);
        p.drawText(QRect(margin, margin + 24, pageW - margin * 2, 25),
                   Qt::AlignCenter, QStringLiteral("Pattern Keeper Import Chart"));
        p.drawText(QRect(margin, margin + 50, pageW - margin * 2, 25),
                   Qt::AlignCenter, QStringLiteral("Page %1").arg(pageNumber));

        const int chartAreaW = pageW - margin * 2;
        const int chartAreaH = pageH - margin * 2 - headerH - footerH;
        const double fitCell = std::min(chartAreaW / std::max(1.0, static_cast<double>(cols)),
                                        chartAreaH / std::max(1.0, static_cast<double>(rows)));
        const int cell = std::max(1, static_cast<int>(std::floor(fitCell)));
        const int gridW = cell * cols;
        const int gridH = cell * rows;
        const int startX = margin + (chartAreaW - gridW) / 2;
        const int startY = margin + headerH;

        auto drawFloatingSymbol = [&](const QRect &cellRect, const QString &symbol, const QColor &penColor) {
            if (cell < 5 || symbol.isEmpty()) return;

            QFont symbolFont = p.font();

            // WinStitch-style PDF symbols:
            // use pixel sizing relative to the actual chart cell so symbols scale with the grid
            // and float inside the square instead of touching the cell borders.
            int symbolPixels = std::max(4, static_cast<int>(std::floor(cell * 0.62)));

            if (symbol.length() > 1) {
                symbolPixels = std::max(3, static_cast<int>(std::floor(cell * 0.48)));
            }

            symbolFont.setPixelSize(symbolPixels);
            symbolFont.setBold(false);
            p.setFont(symbolFont);
            p.setPen(penColor);

            const int pad = std::max(1, cell / 8);
            const QRect textRect = cellRect.adjusted(pad, pad, -pad, -pad);
            p.drawText(textRect, Qt::AlignCenter, symbol);
        };

        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                const QRect r(startX + x * cell, startY + y * cell, cell, cell);
                const int idx = grid[y][x].colorIndex;
                p.fillRect(r, Qt::white);
                if (idx >= 0) {
                    drawFloatingSymbol(r, pkColors[idx].symbol, Qt::black);
                }
            }
        }

        QPen gridPen(QColor(185, 185, 185));
        gridPen.setWidth(1);
        p.setPen(gridPen);
        for (int x = 0; x <= cols; ++x) {
            const int px = startX + x * cell;
            p.drawLine(px, startY, px, startY + gridH);
        }
        for (int y = 0; y <= rows; ++y) {
            const int py = startY + y * cell;
            p.drawLine(startX, py, startX + gridW, py);
        }

        QPen tenPen(QColor(45, 45, 45));
        tenPen.setWidth(2);
        p.setPen(tenPen);
        for (int x = 0; x <= cols; x += 10) {
            const int px = startX + x * cell;
            p.drawLine(px, startY, px, startY + gridH);
        }
        for (int y = 0; y <= rows; y += 10) {
            const int py = startY + y * cell;
            p.drawLine(startX, py, startX + gridW, py);
        }

        if (options.drawCenterLines) {
            QPen centerPen(Qt::red);
            centerPen.setWidth(2);
            p.setPen(centerPen);
            const int cx = startX + (cols * cell) / 2;
            const int cy = startY + (rows * cell) / 2;
            p.drawLine(cx, startY, cx, startY + gridH);
            p.drawLine(startX, cy, startX + gridW, cy);
        }

        QFont labelFont = p.font();
        labelFont.setPointSize(7);
        labelFont.setBold(false);
        p.setFont(labelFont);
        p.setPen(Qt::black);
        for (int x = 10; x <= cols; x += 10) {
            p.drawText(QRect(startX + x * cell - 35, startY - 28, 70, 22),
                       Qt::AlignCenter, QString::number(x));
        }
        for (int y = 10; y <= rows; y += 10) {
            p.drawText(QRect(startX - 60, startY + y * cell - 11, 50, 22),
                       Qt::AlignRight | Qt::AlignVCenter, QString::number(y));
        }

        p.drawText(QRect(margin, pageH - margin - 132, pageW - margin * 2, 25),
                   Qt::AlignCenter, QStringLiteral("Symbol key and thread lengths are on the final page."));
        drawPdfDisclaimer(p, margin, pageW, pageH);
    };

    auto drawThreadLengthsAndSymbolKeyPage = [&]() {
        p.fillRect(QRect(0, 0, pageW, pageH), Qt::white);

        QFont titleFont = p.font();
        titleFont.setPointSize(16);
        titleFont.setBold(true);
        p.setFont(titleFont);
        p.setPen(Qt::black);
        p.drawText(QRect(margin, margin - 5, pageW - margin * 2, 45),
                   Qt::AlignCenter, QStringLiteral("Thread lengths for ") + title);

        const int tableX = margin;
        const int tableW = pageW - margin * 2;
        int y = margin + 55;

        const int headerH = 34;
        const int rowH = 30;
        const int threadW = 160;
        const int descW = tableW - threadW - 145 - 145 - 145 - 120 - 135;
        const int stitchesW = 145;
        const int backsW = 145;
        const int lengthW = 145;
        const int backstitchW = 120;
        const int skeinW = 135;

        QFont headerFont = p.font();
        headerFont.setPointSize(8);
        headerFont.setBold(true);
        p.setFont(headerFont);
        p.setPen(Qt::black);
        p.drawRect(QRect(tableX, y, tableW, headerH));
        p.drawText(QRect(tableX + 4, y, threadW - 8, headerH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Thread\n(DMC_USA)"));
        p.drawText(QRect(tableX + threadW + 4, y, descW - 8, headerH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Description"));
        p.drawText(QRect(tableX + threadW + descW + 4, y, stitchesW - 8, headerH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Stitches"));
        p.drawText(QRect(tableX + threadW + descW + stitchesW + 4, y, backsW - 8, headerH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Backstitches"));
        p.drawText(QRect(tableX + threadW + descW + stitchesW + backsW + 4, y, lengthW - 8, headerH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Length\n(m)"));
        p.drawText(QRect(tableX + threadW + descW + stitchesW + backsW + lengthW + 4, y, backstitchW - 8, headerH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Backs"));
        p.drawText(QRect(tableX + tableW - skeinW + 4, y, skeinW - 8, headerH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Skein\n8.7yard=8m"));

        QFont rowFont = p.font();
        rowFont.setPointSize(8);
        rowFont.setBold(false);
        p.setFont(rowFont);

        y += headerH;
        for (int i = 0; i < pkColors.size(); ++i) {
            const PatternColor &c = pkColors[i];
            p.setPen(QColor(50, 50, 50));
            p.drawRect(QRect(tableX, y, tableW, rowH));

            const double meters = c.stitches * 0.005;
            const double skeins = meters / 8.0;

            p.setPen(Qt::black);
            p.drawText(QRect(tableX + 4, y, threadW - 8, rowH), Qt::AlignLeft | Qt::AlignVCenter, codeForKey(c));
            p.drawText(QRect(tableX + threadW + 4, y, descW - 8, rowH), Qt::AlignLeft | Qt::AlignVCenter, c.dmcName);
            p.drawText(QRect(tableX + threadW + descW + 4, y, stitchesW - 8, rowH), Qt::AlignLeft | Qt::AlignVCenter, QString::number(c.stitches));
            p.drawText(QRect(tableX + threadW + descW + stitchesW + 4, y, backsW - 8, rowH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("0"));
            p.drawText(QRect(tableX + threadW + descW + stitchesW + backsW + 4, y, lengthW - 8, rowH), Qt::AlignLeft | Qt::AlignVCenter, QString::number(meters, 'f', 1));
            p.drawText(QRect(tableX + threadW + descW + stitchesW + backsW + lengthW + 4, y, backstitchW - 8, rowH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("0.0"));
            p.drawText(QRect(tableX + tableW - skeinW + 4, y, skeinW - 8, rowH), Qt::AlignLeft | Qt::AlignVCenter, QString::number(skeins, 'f', 1));
            y += rowH;
        }

        y += 70;

        QFont instTitle = p.font();
        instTitle.setPointSize(14);
        instTitle.setBold(true);
        p.setFont(instTitle);
        p.setPen(Qt::black);
        p.drawText(QRect(margin, y, pageW - margin * 2, 40),
                   Qt::AlignCenter, QStringLiteral("Instructions and Symbol Key for Design \"") + title + QStringLiteral("\""));
        y += 70;

        QPen rulePen(Qt::black);
        rulePen.setWidth(2);
        p.setPen(rulePen);
        p.drawLine(margin, y, pageW - margin, y);
        y += 50;

        QFont bodyFont = p.font();
        bodyFont.setPointSize(9);
        bodyFont.setBold(false);
        p.setFont(bodyFont);
        p.setPen(Qt::black);

        p.drawText(QRect(margin, y, 210, 28), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Fabric:"));
        p.drawText(QRect(margin + 230, y, 220, 28), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("14 count"));
        y += 54;

        p.drawText(QRect(margin, y, 210, 28), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Stitches:"));
        p.drawText(QRect(margin + 230, y, 220, 28), Qt::AlignLeft | Qt::AlignVCenter, QString("%1 x %2").arg(cols).arg(rows));
        p.drawText(QRect(margin + 520, y, 90, 28), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Size:"));
        p.drawText(QRect(margin + 630, y, pageW - margin - 630, 28), Qt::AlignLeft | Qt::AlignVCenter,
                   QString("%1 x %2 inches or %3 x %4 cm")
                   .arg(cols / 14.0, 0, 'f', 2)
                   .arg(rows / 14.0, 0, 'f', 2)
                   .arg(cols / 14.0 * 2.54, 0, 'f', 2)
                   .arg(rows / 14.0 * 2.54, 0, 'f', 2));
        y += 54;

        p.drawText(QRect(margin, y, 210, 28), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Colours:"));
        p.drawText(QRect(margin + 230, y, 220, 28), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("DMC_USA"));
        y += 54;

        p.drawText(QRect(margin, y, pageW - margin * 2, 28), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Use 2 strands of thread for cross stitch"));
        y += 55;

        const int keyX = margin;
        const int keyW = pageW - margin * 2;
        const int keyRowH = 46;
        const int colGap = 70;
        const int halfW = (keyW - colGap) / 2;
        const int symW = 120;
        const int noW = 150;
        const int nameW = halfW - symW - noW;

        p.setPen(Qt::black);
        p.drawRect(QRect(keyX, y, keyW, keyRowH * (1 + ((pkColors.size() + 1) / 2)) + 18));

        QFont keyHeader = p.font();
        keyHeader.setPointSize(8);
        keyHeader.setBold(true);
        p.setFont(keyHeader);
        p.drawText(QRect(keyX + 8, y, symW - 16, keyRowH), Qt::AlignCenter, QStringLiteral("Sym"));
        p.drawText(QRect(keyX + symW + 8, y, noW - 16, keyRowH), Qt::AlignCenter, QStringLiteral("No."));
        p.drawText(QRect(keyX + symW + noW + 8, y, nameW - 16, keyRowH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Colour Name"));

        const int rightX = keyX + halfW + colGap;
        p.drawText(QRect(rightX + 8, y, symW - 16, keyRowH), Qt::AlignCenter, QStringLiteral("Sym"));
        p.drawText(QRect(rightX + symW + 8, y, noW - 16, keyRowH), Qt::AlignCenter, QStringLiteral("No."));
        p.drawText(QRect(rightX + symW + noW + 8, y, nameW - 16, keyRowH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Colour Name"));

        QFont keyFont = p.font();
        keyFont.setPointSize(10);
        keyFont.setBold(false);
        QFont keySymbolFont = keyFont;
        keySymbolFont.setPointSize(13);
        keySymbolFont.setBold(true);

        const int rowsPerColumn = (pkColors.size() + 1) / 2;
        for (int i = 0; i < pkColors.size(); ++i) {
            const bool right = i >= rowsPerColumn;
            const int row = right ? i - rowsPerColumn : i;
            const int x0 = right ? rightX : keyX;
            const int yy = y + keyRowH + row * keyRowH;

            p.setFont(keySymbolFont);
            p.setPen(Qt::black);
            p.drawText(QRect(x0 + 8, yy, symW - 16, keyRowH), Qt::AlignCenter, pkColors[i].symbol);

            p.setFont(keyFont);
            p.drawText(QRect(x0 + symW + 8, yy, noW - 16, keyRowH), Qt::AlignCenter, codeForKey(pkColors[i]));
            p.drawText(QRect(x0 + symW + noW + 8, yy, nameW - 16, keyRowH), Qt::AlignLeft | Qt::AlignVCenter, pkColors[i].dmcName);
        }

        drawPdfDisclaimer(p, margin, pageW, pageH);
    };

    drawChartPage(1);
    pdf.newPage();
    drawThreadLengthsAndSymbolKeyPage();

    p.end();
    return true;
}


bool PatternEngine::renderPdf(const QString &pdfPath,
                              const QString &title,
                              const QVector<QVector<StitchCell>> &grid,
                              const QVector<PatternColor> &colors,
                              int stitched,
                              int unstitched,
                              const PatternOptions &options,
                              QString *error) const {
    Q_UNUSED(error);

    const int rows = grid.size();
    const int cols = rows > 0 ? grid[0].size() : 0;

    QPageLayout::Orientation orientation = QPageLayout::Portrait;
    if (options.pageOrientation == PatternOptions::PageOrientation::Landscape) {
        orientation = QPageLayout::Landscape;
    } else if (options.pageOrientation == PatternOptions::PageOrientation::Auto) {
        orientation = cols > rows ? QPageLayout::Landscape : QPageLayout::Portrait;
    }

    QPdfWriter pdf(pdfPath);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setPageOrientation(orientation);
    pdf.setResolution(300);
    pdf.setCreator("SpriteStitcher v" APP_VERSION);
    pdf.setTitle(title);
    pdf.setPageMargins(QMarginsF(0.35, 0.35, 0.35, 0.35), QPageLayout::Inch);

    QPainter p(&pdf);
    if (!p.isActive()) {
        if (error) *error = "Could not start PDF painter.";
        return false;
    }

    const int pageW = pdf.width();
    const int pageH = pdf.height();
    const int margin = 90;
    const int footerH = 155;
    const bool useCompactLegend = options.legendPlacement == PatternOptions::LegendPlacement::SamePageWhenPossible && colors.size() <= 12;
    const int compactLegendH = useCompactLegend ? 300 : 0;
    const QString colorChartType = QStringLiteral("Color chart with symbols");
    const QString symbolChartType = QStringLiteral("Pattern Keeper-friendly black-and-white symbols");

    auto gridSizeName = [&]() -> QString {
        switch (options.gridSize) {
            case PatternOptions::GridSize::Small: return QStringLiteral("Small");
            case PatternOptions::GridSize::Large: return QStringLiteral("Large");
            case PatternOptions::GridSize::Medium:
            default: return QStringLiteral("Medium");
        }
    };

    auto legendPlacementName = [&]() -> QString {
        if (options.legendPlacement == PatternOptions::LegendPlacement::SamePageWhenPossible) {
            return useCompactLegend ? QStringLiteral("Same page") : QStringLiteral("Separate page; too many colors for same-page legend");
        }
        return QStringLiteral("Separate page");
    };

    auto orientationName = [&]() -> QString {
        if (orientation == QPageLayout::Landscape) return QStringLiteral("Landscape");
        return QStringLiteral("Portrait");
    };

    auto maxCellForGridSize = [&]() -> int {
        switch (options.gridSize) {
            case PatternOptions::GridSize::Small: return 20;
            case PatternOptions::GridSize::Large: return 42;
            case PatternOptions::GridSize::Medium:
            default: return 30;
        }
    };

    QFont titleFont = p.font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);

    QFont panelHeadingFont = p.font();
    panelHeadingFont.setPointSize(10);
    panelHeadingFont.setBold(true);

    QFont panelBodyFont = p.font();
    panelBodyFont.setPointSize(9);
    panelBodyFont.setBold(false);

    const double w14 = cols / 14.0;
    const double h14 = rows / 14.0;
    const double w16 = cols / 16.0;
    const double h16 = rows / 16.0;
    const double w18 = cols / 18.0;
    const double h18 = rows / 18.0;

    auto chartPanelLines = [&](const QString &chartType) -> QStringList {
        return {
            QString("Chart type: %1").arg(chartType),
            QString("Grid size: %1 x %2 stitches").arg(cols).arg(rows),
            QString("Stitched count: %1").arg(stitched),
            QString("Background/unstitched count: %1").arg(unstitched),
            QString("Color count: %1").arg(colors.size())
        };
    };

    auto fabricPanelLines = [&]() -> QStringList {
        return {
            QString("14ct size: %1 x %2 in").arg(w14, 0, 'f', 2).arg(h14, 0, 'f', 2),
            QString("16ct size: %1 x %2 in").arg(w16, 0, 'f', 2).arg(h16, 0, 'f', 2),
            QString("18ct size: %1 x %2 in").arg(w18, 0, 'f', 2).arg(h18, 0, 'f', 2)
        };
    };

    const int panelPaddingX = 26;
    const int panelPaddingY = 24;
    const int panelGap = 34;
    const int titlePanelGap = 28;
    const int panelGridGap = 92;
    const int headingBodyGap = 12;

    auto wrappedLineHeight = [&](const QFont &font, const QString &text, int width) -> int {
        QFontMetrics metrics(font, p.device());
        const QRect bounds = metrics.boundingRect(QRect(0, 0, width, 10000),
                                                  Qt::AlignLeft | Qt::TextWordWrap,
                                                  text);
        return std::max(metrics.lineSpacing(), bounds.height()) + 4;
    };

    const int panelW = (pageW - margin * 2 - panelGap) / 2;
    const int panelTextW = std::max(1, panelW - panelPaddingX * 2);

    auto measurePanelHeight = [&](const QStringList &lines) -> int {
        QFontMetrics headingMetrics(panelHeadingFont, p.device());
        int height = panelPaddingY * 2 + headingMetrics.lineSpacing() + headingBodyGap;
        for (const QString &line : lines) {
            height += wrappedLineHeight(panelBodyFont, line, panelTextW);
        }
        return height;
    };

    struct ChartHeaderLayout {
        QRect titleRect;
        QRect chartPanelRect;
        QRect fabricPanelRect;
        int gridTopY;
    };

    auto makeChartHeaderLayout = [&]() -> ChartHeaderLayout {
        QFontMetrics titleMetrics(titleFont, p.device());
        const int titleTextH = titleMetrics.boundingRect(QRect(0, 0, pageW - margin * 2, 10000),
                                                         Qt::AlignLeft | Qt::TextWordWrap,
                                                         title).height();
        const int titleH = std::max(titleMetrics.lineSpacing(), titleTextH) + 18;
        const int panelTop = margin + titleH + titlePanelGap;
        const int panelH = std::max({
            measurePanelHeight(chartPanelLines(colorChartType)),
            measurePanelHeight(chartPanelLines(symbolChartType)),
            measurePanelHeight(fabricPanelLines())
        });

        ChartHeaderLayout layout;
        layout.titleRect = QRect(margin, margin, pageW - margin * 2, titleH);
        layout.chartPanelRect = QRect(margin, panelTop, panelW, panelH);
        layout.fabricPanelRect = QRect(margin + panelW + panelGap, panelTop, panelW, panelH);
        layout.gridTopY = panelTop + panelH + panelGridGap;
        return layout;
    };

    const ChartHeaderLayout chartHeaderLayout = makeChartHeaderLayout();

    auto drawInfoPanel = [&](const QRect &rect, const QString &heading, const QStringList &lines) {
        p.fillRect(rect, QColor(246, 246, 246));
        p.setPen(QColor(185, 185, 185));
        p.drawRect(rect);

        const QRect contentRect = rect.adjusted(panelPaddingX, panelPaddingY, -panelPaddingX, -panelPaddingY);
        QFontMetrics headingMetrics(panelHeadingFont, p.device());

        p.setPen(Qt::black);
        p.setFont(panelHeadingFont);
        p.drawText(QRect(contentRect.x(), contentRect.y(), contentRect.width(), headingMetrics.lineSpacing()),
                   Qt::AlignLeft | Qt::AlignTop,
                   heading);

        p.setFont(panelBodyFont);
        int y = contentRect.y() + headingMetrics.lineSpacing() + headingBodyGap;
        for (const QString &line : lines) {
            const int lineH = wrappedLineHeight(panelBodyFont, line, contentRect.width());
            p.drawText(QRect(contentRect.x(), y, contentRect.width(), lineH),
                       Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                       line);
            y += lineH;
        }
    };

    auto drawHeader = [&](const QString &chartType) -> int {
        p.setPen(Qt::black);
        p.setFont(titleFont);
        p.drawText(chartHeaderLayout.titleRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, title);

        drawInfoPanel(chartHeaderLayout.chartPanelRect, QStringLiteral("Chart"), chartPanelLines(chartType));
        drawInfoPanel(chartHeaderLayout.fabricPanelRect, QStringLiteral("Fabric sizes"), fabricPanelLines());
        return chartHeaderLayout.gridTopY;
    };

    auto drawCompactLegend = [&](int topY) {
        if (!useCompactLegend) return;

        const int legendX = margin;
        const int legendW = pageW - margin * 2;
        const int titleH = 38;
        const int rowH = 38;
        const int colW = legendW / 2;
        const int rowsPerColumn = std::max(1, (static_cast<int>(colors.size()) + 1) / 2);

        p.setPen(Qt::black);
        QFont titleFont = p.font();
        titleFont.setPointSize(10);
        titleFont.setBold(true);
        p.setFont(titleFont);
        p.drawText(QRect(legendX, topY, legendW, titleH), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("DMC Legend / Shopping List"));

        QFont rowFont = p.font();
        rowFont.setPointSize(8);
        rowFont.setBold(false);
        p.setFont(rowFont);

        for (int i = 0; i < colors.size(); ++i) {
            const int col = i / rowsPerColumn;
            const int row = i % rowsPerColumn;
            const int x = legendX + col * colW;
            const int y = topY + titleH + row * rowH;
            const QRect rowRect(x, y, colW - 12, rowH);
            p.setPen(QColor(200, 200, 200));
            p.drawRect(rowRect);

            const QRect swatch(x + 8, y + 8, 22, 22);
            p.fillRect(swatch, colors[i].color);
            p.setPen(Qt::black);
            p.drawRect(swatch);

            const QString text = QString("%1  DMC %2  %3  (%4)")
                    .arg(colors[i].symbol)
                    .arg(colors[i].dmcCode)
                    .arg(colors[i].dmcName)
                    .arg(colors[i].stitches);
            p.drawText(QRect(x + 38, y, colW - 54, rowH), Qt::AlignLeft | Qt::AlignVCenter, text);
        }
    };

    auto drawCoverPage = [&]() {
        p.fillRect(QRect(0, 0, pageW, pageH), Qt::white);

        p.setPen(Qt::black);
        QFont titleFont = p.font();
        titleFont.setPointSize(24);
        titleFont.setBold(true);
        p.setFont(titleFont);
        p.drawText(QRect(margin, margin + 60, pageW - margin * 2, 90), Qt::AlignLeft | Qt::AlignVCenter, title);

        QFont subFont = p.font();
        subFont.setPointSize(13);
        subFont.setBold(false);
        p.setFont(subFont);
        p.drawText(QRect(margin, margin + 165, pageW - margin * 2, 45), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Cross-stitch pattern generated by SpriteStitcher v2.9.8"));

        QFont bodyFont = p.font();
        bodyFont.setPointSize(11);
        p.setFont(bodyFont);
        QStringList lines;
        lines << QString("Grid: %1 x %2 stitches").arg(cols).arg(rows);
        lines << QString("Total squares: %1").arg(cols * rows);
        lines << QString("Stitched squares: %1").arg(stitched);
        lines << QString("Unstitched/background squares: %1").arg(unstitched);
        lines << QString("Colors: %1").arg(colors.size());
        lines << QString("Finished size on 14ct Aida: %1 x %2 in").arg(cols / 14.0, 0, 'f', 2).arg(rows / 14.0, 0, 'f', 2);
        lines << QString("Finished size on 16ct Aida: %1 x %2 in").arg(cols / 16.0, 0, 'f', 2).arg(rows / 16.0, 0, 'f', 2);
        lines << QString("Finished size on 18ct Aida: %1 x %2 in").arg(cols / 18.0, 0, 'f', 2).arg(rows / 18.0, 0, 'f', 2);
        lines << QString("Grid size option: %1").arg(gridSizeName());
        lines << QString("Legend placement: %1").arg(legendPlacementName());
        lines << QString("Page orientation: %1").arg(orientationName());
        lines << QString("Center lines: %1").arg(options.drawCenterLines ? QStringLiteral("On") : QStringLiteral("Off"));

        p.drawText(QRect(margin, margin + 260, pageW - margin * 2, pageH - margin * 2 - 260),
                   Qt::AlignLeft | Qt::AlignTop, lines.join('\n'));
        drawPdfDisclaimer(p, margin, pageW, pageH);
    };

    auto drawChart = [&](bool colorChart) {
        p.fillRect(QRect(0, 0, pageW, pageH), Qt::white);
        const int startY = drawHeader(colorChart ? colorChartType : symbolChartType);

        const int chartAreaW = pageW - margin * 2;
        const int chartAreaH = std::max(1, pageH - startY - margin - footerH - compactLegendH);
        const double fitCell = std::min(chartAreaW / std::max(1.0, static_cast<double>(cols)),
                                        chartAreaH / std::max(1.0, static_cast<double>(rows)));
        const int cell = std::max(1, static_cast<int>(std::floor(std::min(fitCell, static_cast<double>(maxCellForGridSize())))));
        const int gridW = cell * cols;
        const int gridH = cell * rows;
        const int startX = margin + (chartAreaW - gridW) / 2;

        auto drawFloatingSymbol = [&](const QRect &cellRect, const QString &symbol, const QColor &penColor) {
            if (cell < 5 || symbol.isEmpty()) return;

            QFont symbolFont = p.font();

            // WinStitch-style PDF symbols:
            // use pixel sizing relative to the actual chart cell so symbols scale with the grid
            // and float inside the square instead of touching the cell borders.
            int symbolPixels = std::max(4, static_cast<int>(std::floor(cell * 0.62)));

            if (symbol.length() > 1) {
                symbolPixels = std::max(3, static_cast<int>(std::floor(cell * 0.48)));
            }

            symbolFont.setPixelSize(symbolPixels);
            symbolFont.setBold(false);
            p.setFont(symbolFont);
            p.setPen(penColor);

            const int pad = std::max(1, cell / 8);
            const QRect textRect = cellRect.adjusted(pad, pad, -pad, -pad);
            p.drawText(textRect, Qt::AlignCenter, symbol);
        };

        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                const QRect r(startX + x * cell, startY + y * cell, cell, cell);
                const int idx = grid[y][x].colorIndex;
                if (idx >= 0) {
                    QColor symbolColor = Qt::black;
                    if (colorChart) {
                        p.fillRect(r, colors[idx].color);
                        int gray = qGray(colors[idx].color.rgb());
                        symbolColor = gray < 120 ? Qt::white : Qt::black;
                    } else {
                        p.fillRect(r, Qt::white);
                    }
                    drawFloatingSymbol(r, colors[idx].symbol, symbolColor);
                } else {
                    p.fillRect(r, Qt::white);
                }
            }
        }

        QPen gridPen(QColor(190, 190, 190));
        gridPen.setWidth(1);
        p.setPen(gridPen);
        for (int x = 0; x <= cols; ++x) {
            int px = startX + x * cell;
            p.drawLine(px, startY, px, startY + gridH);
        }
        for (int y = 0; y <= rows; ++y) {
            int py = startY + y * cell;
            p.drawLine(startX, py, startX + gridW, py);
        }

        QPen tenPen(QColor(80, 80, 80));
        tenPen.setWidth(2);
        p.setPen(tenPen);
        for (int x = 0; x <= cols; x += 10) {
            int px = startX + x * cell;
            p.drawLine(px, startY, px, startY + gridH);
        }
        for (int y = 0; y <= rows; y += 10) {
            int py = startY + y * cell;
            p.drawLine(startX, py, startX + gridW, py);
        }

        if (options.drawCenterLines) {
            QPen centerPen(Qt::red);
            centerPen.setWidth(3);
            p.setPen(centerPen);
            const int cx = startX + (cols * cell) / 2;
            const int cy = startY + (rows * cell) / 2;
            p.drawLine(cx, startY, cx, startY + gridH);
            p.drawLine(startX, cy, startX + gridW, cy);
        }

        QFont labelFont = p.font();
        labelFont.setPointSize(7);
        labelFont.setBold(false);
        p.setFont(labelFont);
        p.setPen(Qt::black);
        for (int x = 0; x <= cols; x += 10) {
            QString n = QString::number(x == 0 ? 1 : x);
            p.drawText(QRect(startX + x * cell - 40, startY - 35, 80, 28), Qt::AlignCenter, n);
        }
        for (int y = 0; y <= rows; y += 10) {
            QString n = QString::number(y == 0 ? 1 : y);
            p.drawText(QRect(startX - 75, startY + y * cell - 14, 55, 28), Qt::AlignRight | Qt::AlignVCenter, n);
        }

        if (useCompactLegend) drawCompactLegend(startY + gridH + 30);

        QFont footerFont = p.font();
        footerFont.setPointSize(8);
        footerFont.setBold(false);
        p.setFont(footerFont);
        p.setPen(Qt::black);
        const QString footer = useCompactLegend
                ? QStringLiteral("DMC legend is included on this page. Entries are sorted by DMC number.")
                : QStringLiteral("Full DMC legend / shopping list starts on a following page. Legend entries are sorted by DMC number.");
        p.drawText(QRect(margin, pageH - margin - 132, pageW - margin * 2, 28), Qt::AlignLeft | Qt::AlignVCenter, footer);
        drawPdfDisclaimer(p, margin, pageW, pageH);
    };

    auto drawLegendPage = [&](int startIndex) -> int {
        p.fillRect(QRect(0, 0, pageW, pageH), Qt::white);

        p.setPen(Qt::black);
        QFont titleFont = p.font();
        titleFont.setPointSize(18);
        titleFont.setBold(true);
        p.setFont(titleFont);
        p.drawText(QRect(margin, margin, pageW - margin * 2, 55), Qt::AlignLeft | Qt::AlignVCenter,
                   title + QStringLiteral(" — DMC Legend / Shopping List"));

        QFont infoFont = p.font();
        infoFont.setPointSize(9);
        infoFont.setBold(false);
        p.setFont(infoFont);
        p.drawText(QRect(margin, margin + 65, pageW - margin * 2, 40), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Sorted by DMC number. One symbol maps to one stitched color. Blank chart squares are unstitched background."));

        if (colors.isEmpty()) {
            QFont emptyFont = p.font();
            emptyFont.setPointSize(12);
            emptyFont.setBold(false);
            p.setFont(emptyFont);
            p.drawText(QRect(margin, margin + 140, pageW - margin * 2, 60), Qt::AlignLeft | Qt::AlignVCenter,
                       QStringLiteral("No stitched colors were found."));
            return 0;
        }

        const int tableX = margin;
        const int tableY = margin + 130;
        const int tableW = pageW - margin * 2;
        const int headerRowH = 38;
        const int rowH = 46;
        const int rowsPerPage = std::max(1, (pageH - tableY - headerRowH - margin - 45) / rowH);

        const int symbolW = 150;
        const int swatchW = 105;
        const int dmcW = 150;
        const int stitchW = 135;
        const int nameW = tableW - symbolW - swatchW - dmcW - stitchW;

        QFont headerFont = p.font();
        headerFont.setPointSize(9);
        headerFont.setBold(true);
        p.setFont(headerFont);
        p.fillRect(QRect(tableX, tableY, tableW, headerRowH), QColor(235, 235, 235));
        p.setPen(Qt::black);
        p.drawRect(QRect(tableX, tableY, tableW, headerRowH));
        p.drawText(QRect(tableX + 8, tableY, symbolW - 16, headerRowH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Symbol"));
        p.drawText(QRect(tableX + symbolW + 8, tableY, swatchW - 16, headerRowH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Color"));
        p.drawText(QRect(tableX + symbolW + swatchW + 8, tableY, dmcW - 16, headerRowH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("DMC"));
        p.drawText(QRect(tableX + symbolW + swatchW + dmcW + 8, tableY, nameW - 16, headerRowH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Description"));
        p.drawText(QRect(tableX + symbolW + swatchW + dmcW + nameW + 8, tableY, stitchW - 16, headerRowH), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("Stitches"));

        QFont rowFont = p.font();
        rowFont.setPointSize(9);
        rowFont.setBold(false);
        p.setFont(rowFont);

        int i = startIndex;
        for (int row = 0; row < rowsPerPage && i < colors.size(); ++row, ++i) {
            const int y = tableY + headerRowH + row * rowH;
            const QRect rowRect(tableX, y, tableW, rowH);
            p.setPen(QColor(190, 190, 190));
            p.drawRect(rowRect);

            p.setPen(Qt::black);
            p.drawText(QRect(tableX + 8, y, symbolW - 16, rowH), Qt::AlignLeft | Qt::AlignVCenter, colors[i].symbol);

            const QRect swatch(tableX + symbolW + 22, y + 9, 28, 28);
            p.fillRect(swatch, colors[i].color);
            p.setPen(Qt::black);
            p.drawRect(swatch);

            p.drawText(QRect(tableX + symbolW + swatchW + 8, y, dmcW - 16, rowH), Qt::AlignLeft | Qt::AlignVCenter, colors[i].dmcCode);
            p.drawText(QRect(tableX + symbolW + swatchW + dmcW + 8, y, nameW - 16, rowH), Qt::AlignLeft | Qt::AlignVCenter, colors[i].dmcName);
            p.drawText(QRect(tableX + symbolW + swatchW + dmcW + nameW + 8, y, stitchW - 16, rowH), Qt::AlignRight | Qt::AlignVCenter, QString::number(colors[i].stitches));
        }

        QFont footerFont = p.font();
        footerFont.setPointSize(8);
        footerFont.setBold(false);
        p.setFont(footerFont);
        p.setPen(Qt::black);
        p.drawText(QRect(margin, pageH - margin - 132, pageW - margin * 2, 28), Qt::AlignRight | Qt::AlignVCenter,
                   QString("Legend entries %1-%2 of %3").arg(startIndex + 1).arg(i).arg(colors.size()));
        drawPdfDisclaimer(p, margin, pageW, pageH);
        return i;
    };


    auto drawPatternKeeperThreadKeyPage = [&](int startIndex) -> int {
        p.fillRect(QRect(0, 0, pageW, pageH), Qt::white);

        p.setPen(Qt::black);
        QFont titleFont = p.font();
        titleFont.setPointSize(18);
        titleFont.setBold(true);
        p.setFont(titleFont);
        p.drawText(QRect(margin, margin, pageW - margin * 2, 55), Qt::AlignLeft | Qt::AlignVCenter,
                   title + QStringLiteral(" — Pattern Keeper Thread Key"));

        QFont infoFont = p.font();
        infoFont.setPointSize(9);
        infoFont.setBold(false);
        p.setFont(infoFont);
        p.drawText(QRect(margin, margin + 65, pageW - margin * 2, 82), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                   QStringLiteral("Plain import key. Each entry is intentionally spaced out to prevent text overlap: SYMBOL  |  DMC code  |  floss name  |  stitch count."));

        if (colors.isEmpty()) {
            QFont emptyFont = p.font();
            emptyFont.setPointSize(12);
            emptyFont.setBold(false);
            p.setFont(emptyFont);
            p.drawText(QRect(margin, margin + 165, pageW - margin * 2, 60), Qt::AlignLeft | Qt::AlignVCenter,
                       QStringLiteral("No stitched colors were found."));
            return 0;
        }

        const int tableX = margin;
        const int tableY = margin + 165;
        const int tableW = pageW - margin * 2;
        const int headerRowH = 48;

        // At 300 DPI, point sizes use more device pixels than expected.
        // Keep the rows tall so symbols and text never collide.
        const int rowH = 96;
        const int rowsPerPage = std::max(1, (pageH - tableY - headerRowH - margin - 50) / rowH);

        const int symbolW = 190;
        const int dmcW = 230;
        const int stitchW = 165;
        const int nameW = tableW - symbolW - dmcW - stitchW;

        QFont headerFont = p.font();
        headerFont.setPointSize(10);
        headerFont.setBold(true);
        p.setFont(headerFont);
        p.setPen(Qt::black);
        p.drawText(QRect(tableX + 8, tableY, symbolW - 16, headerRowH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("SYMBOL"));
        p.drawText(QRect(tableX + symbolW + 8, tableY, dmcW - 16, headerRowH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("DMC"));
        p.drawText(QRect(tableX + symbolW + dmcW + 8, tableY, nameW - 16, headerRowH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("FLOSS NAME"));
        p.drawText(QRect(tableX + symbolW + dmcW + nameW + 8, tableY, stitchW - 16, headerRowH), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("STITCHES"));

        QPen linePen(QColor(175, 175, 175));
        linePen.setWidth(2);
        p.setPen(linePen);
        p.drawLine(tableX, tableY + headerRowH, tableX + tableW, tableY + headerRowH);

        int i = startIndex;
        for (int row = 0; row < rowsPerPage && i < colors.size(); ++row, ++i) {
            const int y = tableY + headerRowH + row * rowH;

            QPen rowPen(QColor(220, 220, 220));
            rowPen.setWidth(1);
            p.setPen(rowPen);
            p.drawLine(tableX, y + rowH, tableX + tableW, y + rowH);

            QFont symbolFont = p.font();
            symbolFont.setPointSize(14);
            symbolFont.setBold(true);
            p.setFont(symbolFont);
            p.setPen(Qt::black);
            p.drawText(QRect(tableX + 8, y, symbolW - 16, rowH), Qt::AlignLeft | Qt::AlignVCenter, colors[i].symbol);

            QFont rowFont = p.font();
            rowFont.setPointSize(10);
            rowFont.setBold(false);
            p.setFont(rowFont);

            QString dmcText = QStringLiteral("DMC ") + colors[i].dmcCode;
            if (colors[i].dmcCode.compare(QStringLiteral("White"), Qt::CaseInsensitive) == 0) {
                dmcText = QStringLiteral("DMC White / Blanc");
            } else if (colors[i].dmcCode.compare(QStringLiteral("B5200"), Qt::CaseInsensitive) == 0) {
                dmcText = QStringLiteral("DMC B5200");
            }

            p.drawText(QRect(tableX + symbolW + 8, y, dmcW - 16, rowH), Qt::AlignLeft | Qt::AlignVCenter, dmcText);
            p.drawText(QRect(tableX + symbolW + dmcW + 8, y, nameW - 16, rowH), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, colors[i].dmcName);
            p.drawText(QRect(tableX + symbolW + dmcW + nameW + 8, y, stitchW - 16, rowH), Qt::AlignRight | Qt::AlignVCenter, QString::number(colors[i].stitches));
        }

        QFont footerFont = p.font();
        footerFont.setPointSize(8);
        footerFont.setBold(false);
        p.setFont(footerFont);
        p.setPen(Qt::black);
        p.drawText(QRect(margin, pageH - margin - 132, pageW - margin * 2, 28), Qt::AlignRight | Qt::AlignVCenter,
                   QString("Pattern Keeper thread key entries %1-%2 of %3").arg(startIndex + 1).arg(i).arg(colors.size()));
        drawPdfDisclaimer(p, margin, pageW, pageH);
        return i;
    };

    bool pageStarted = false;
    auto startNewPageIfNeeded = [&]() {
        if (pageStarted) pdf.newPage();
        pageStarted = true;
    };

    if (options.includeCoverPage) {
        startNewPageIfNeeded();
        drawCoverPage();
    }

    bool madeAnyChart = false;
    if (options.writeColorChart) {
        startNewPageIfNeeded();
        drawChart(true);
        madeAnyChart = true;
    }
    if (options.writeSymbolChart) {
        startNewPageIfNeeded();
        drawChart(false);
        madeAnyChart = true;
    }
    if (!madeAnyChart) {
        startNewPageIfNeeded();
        drawChart(true);
    }

    if (!useCompactLegend) {
        startNewPageIfNeeded();
        int legendIndex = 0;
        do {
            legendIndex = drawLegendPage(legendIndex);
            if (legendIndex < colors.size()) pdf.newPage();
        } while (legendIndex < colors.size());
    }

    p.end();
    return true;
}



PatternData PatternEngine::patternDataForImage(const QString &imagePath, const PatternOptions &options) const {
    PatternData data;

    QImage src(imagePath);
    if (src.isNull()) {
        data.summary.message = "Could not load image: " + imagePath;
        return data;
    }

    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    int stitched = 0;
    int unstitched = 0;
    int colorCountBeforeCleanup = 0;
    buildPattern(img, options, data.grid, data.colors, stitched, unstitched, &colorCountBeforeCleanup);

    data.summary.ok = true;
    data.summary.width = img.width();
    data.summary.height = img.height();
    data.summary.stitched = stitched;
    data.summary.unstitched = unstitched;
    data.summary.colorCount = data.colors.size();
    data.summary.colorCountBeforeCleanup = colorCountBeforeCleanup;
    data.summary.message = QString("Built %1 x %2 chart preview with %3 stitched squares and %4 colors.")
            .arg(data.summary.width).arg(data.summary.height).arg(data.summary.stitched).arg(data.summary.colorCount);
    return data;
}



QVector<PatternColor> PatternEngine::paletteForImage(const QString &imagePath, const PatternOptions &options, PatternResult *summary) const {
    if (summary) *summary = PatternResult();

    QImage src(imagePath);
    if (src.isNull()) {
        if (summary) summary->message = "Could not load image: " + imagePath;
        return {};
    }

    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    QVector<QVector<StitchCell>> grid;
    QVector<PatternColor> colors;
    int stitched = 0;
    int unstitched = 0;
    int colorCountBeforeCleanup = 0;
    buildPattern(img, options, grid, colors, stitched, unstitched, &colorCountBeforeCleanup);

    if (summary) {
        summary->ok = true;
        summary->width = img.width();
        summary->height = img.height();
        summary->stitched = stitched;
        summary->unstitched = unstitched;
        summary->colorCount = colors.size();
        summary->colorCountBeforeCleanup = colorCountBeforeCleanup;
        summary->message = QString("Palette contains %1 stitched colors after cleanup.").arg(colors.size());
    }

    return colors;
}

PatternResult PatternEngine::analyzeImage(const QString &imagePath, const PatternOptions &options) const {
    PatternResult result;
    QImage src(imagePath);
    if (src.isNull()) {
        result.message = "Could not load image. Check that the file exists and is a supported image: " + imagePath;
        return result;
    }

    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    QVector<QVector<StitchCell>> grid;
    QVector<PatternColor> colors;
    int stitched = 0;
    int unstitched = 0;
    int colorCountBeforeCleanup = 0;
    buildPattern(img, options, grid, colors, stitched, unstitched, &colorCountBeforeCleanup);

    result.ok = true;
    result.width = img.width();
    result.height = img.height();
    result.stitched = stitched;
    result.unstitched = unstitched;
    result.colorCount = colors.size();
    result.colorCountBeforeCleanup = colorCountBeforeCleanup;
    result.message = QString("Analyzed %1 x %2 sprite with %3 stitched squares and %4 colors.")
            .arg(result.width).arg(result.height).arg(result.stitched).arg(result.colorCount);
    return result;
}

PatternResult PatternEngine::generatePdf(const QString &imagePath, const PatternOptions &options) {
    PatternResult result;
    QImage src(imagePath);
    if (src.isNull()) {
        result.message = "Could not load image. Check that the file exists and is a supported image: " + imagePath;
        return result;
    }

    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    QVector<QVector<StitchCell>> grid;
    QVector<PatternColor> colors;
    int stitched = 0;
    int unstitched = 0;
    int colorCountBeforeCleanup = 0;
    buildPattern(img, options, grid, colors, stitched, unstitched, &colorCountBeforeCleanup);

    QDir outDir(options.outputDir.isEmpty() ? QFileInfo(imagePath).absolutePath() : options.outputDir);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        result.message = "Could not create output folder: " + outDir.absolutePath();
        return result;
    }
    if (!QFileInfo(outDir.absolutePath()).isDir()) {
        result.message = "Output path exists but is not a folder: " + outDir.absolutePath();
        return result;
    }
    if (!QFileInfo(outDir.absolutePath()).isWritable()) {
        result.message = "Output folder is not writable: " + outDir.absolutePath();
        return result;
    }

    QString title = options.title.trimmed();
    if (title.isEmpty()) title = QFileInfo(imagePath).completeBaseName();
    const QString base = safeFileBase(title);
    const QString pdfPath = uniqueFilePath(outDir, base + "_cross_stitch_v2_9_6", ".pdf");
    const QString csvPath = uniqueFilePath(outDir, base + "_legend_v2_9_6", ".csv");
    const QString pkPdfPath = uniqueFilePath(outDir, base + "_pattern_keeper_import_v2_9_6", ".pdf");
    const QString previewPngPath = uniqueFilePath(outDir, base + "_website_preview_v2_9_6", ".png");

    QString error;
    if (!renderPdf(pdfPath, title + " Cross Stitch Pattern", grid, colors, stitched, unstitched, options, &error)) {
        result.message = "Could not write PDF: " + pdfPath + (error.isEmpty() ? QString() : QStringLiteral("\n") + error);
        return result;
    }

    if (options.writeSymbolChart) {
        QString pkError;
        if (!renderPatternKeeperImportPdf(pkPdfPath, title + " Cross Stitch Pattern", grid, colors, stitched, unstitched, options, &pkError)) {
            result.message = "PDF written, but Pattern Keeper import PDF failed: " + pkPdfPath
                    + (pkError.isEmpty() ? QString() : QStringLiteral("\n") + pkError);
        } else {
            result.patternKeeperPdfPath = pkPdfPath;
        }
    }

    if (options.writeLegendCsv) {
        if (!writeLegendCsv(csvPath, colors, &error)) {
            result.message = "PDF written, but CSV failed: " + csvPath + (error.isEmpty() ? QString() : QStringLiteral("\n") + error);
        }
    }

    if (options.writePreviewPng) {
        QString pngError;
        if (!renderPreviewPng(previewPngPath, grid, colors, &pngError)) {
            result.message = "PDF written, but website preview PNG failed: " + previewPngPath
                    + (pngError.isEmpty() ? QString() : QStringLiteral("\n") + pngError);
        } else {
            result.previewPngPath = previewPngPath;
        }
    }

    result.ok = true;
    result.pdfPath = pdfPath;
    result.csvPath = options.writeLegendCsv ? csvPath : QString();
    result.width = img.width();
    result.height = img.height();
    result.stitched = stitched;
    result.unstitched = unstitched;
    result.colorCount = colors.size();
    result.colorCountBeforeCleanup = colorCountBeforeCleanup;
    result.message = QString("Generated %1 x %2 pattern with %3 stitched squares and %4 colors.")
            .arg(result.width).arg(result.height).arg(result.stitched).arg(result.colorCount);
    if (!result.patternKeeperPdfPath.isEmpty()) {
        result.message += "\nPattern Keeper import PDF: " + result.patternKeeperPdfPath;
    }
    if (!result.previewPngPath.isEmpty()) {
        result.message += "\nWebsite preview PNG: " + result.previewPngPath;
    }
    return result;
}
