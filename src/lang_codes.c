/*
 *  Multi-language Support - language codes
 *  Copyright (C) 2012 Adam Sutton
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdlib.h>

#include "lang_codes.h"
#include "config.h"

/* **************************************************************************
 * Code list
 * *************************************************************************/

const lang_code_t lang_codes[] = {
  { "und", NULL, NULL , "Undetermined" },
  { "aar", "aa", NULL , "Afar" },
  { "abk", "ab", NULL , "Abkhazian" },
  { "ace", NULL, NULL , "Achinese" },
  { "ach", NULL, NULL , "Acoli" },
  { "ada", NULL, NULL , "Adangme" },
  { "ady", NULL, NULL , "Adyghe; Adygei" },
  { "afa", NULL, NULL , "Afro-Asiatic languages" },
  { "afh", NULL, NULL , "Afrihili" },
  { "afr", "af", NULL , "Afrikaans" },
  { "ain", NULL, NULL , "Ainu" },
  { "aka", "ak", NULL , "Akan" },
  { "akk", NULL, NULL , "Akkadian" },
  { "alb", "sq", "sqi", "Albanian" },
  { "ale", NULL, NULL , "Aleut" },
  { "alg", NULL, NULL , "Algonquian languages" },
  { "alt", NULL, NULL , "Southern Altai" },
  { "amh", "am", NULL , "Amharic" },
  { "anp", NULL, NULL , "Angika" },
  { "apa", NULL, NULL , "Apache languages" },
  { "ara", "ar", NULL , "Arabic" },
  { "arc", NULL, NULL , "Official Aramaic (700-300 BCE); Imperial Aramaic (700-300 BCE)" },
  { "arg", "an", NULL , "Aragonese" },
  { "arm", "hy", "hye", "Armenian" },
  { "arn", NULL, NULL , "Mapudungun; Mapuche" },
  { "arp", NULL, NULL , "Arapaho" },
  { "art", NULL, NULL , "Artificial languages" },
  { "arw", NULL, NULL , "Arawak" },
  { "asm", "as", NULL , "Assamese" },
  { "ast", NULL, NULL , "Asturian; Bable; Leonese; Asturleonese" },
  { "ath", NULL, NULL , "Athapascan languages" },
  { "aus", NULL, NULL , "Australian languages" },
  { "ava", "av", NULL , "Avaric" },
  { "ave", "ae", NULL , "Avestan" },
  { "awa", NULL, NULL , "Awadhi" },
  { "aym", "ay", NULL , "Aymara" },
  { "aze", "az", NULL , "Azerbaijani" },
  { "bad", NULL, NULL , "Banda languages" },
  { "bai", NULL, NULL , "Bamileke languages" },
  { "bak", "ba", NULL , "Bashkir" },
  { "bal", NULL, NULL , "Baluchi" },
  { "bam", "bm", NULL , "Bambara" },
  { "ban", NULL, NULL , "Balinese" },
  { "baq", "eu", "eus", "Basque" },
  { "bas", NULL, NULL , "Basa" },
  { "bat", NULL, NULL , "Baltic languages" },
  { "bej", NULL, NULL , "Beja; Bedawiyet" },
  { "bel", "be", NULL , "Belarusian" },
  { "bem", NULL, NULL , "Bemba" },
  { "ben", "bn", NULL , "Bengali" },
  { "ber", NULL, NULL , "Berber languages" },
  { "bho", NULL, NULL , "Bhojpuri" },
  { "bih", "bh", NULL , "Bihari languages" },
  { "bik", NULL, NULL , "Bikol" },
  { "bin", NULL, NULL , "Bini; Edo" },
  { "bis", "bi", NULL , "Bislama" },
  { "bla", NULL, NULL , "Siksika" },
  { "bnt", NULL, NULL , "Bantu languages" },
  { "bos", "bs", NULL , "Bosnian" },
  { "bra", NULL, NULL , "Braj" },
  { "bre", "br", NULL , "Breton" },
  { "btk", NULL, NULL , "Batak languages" },
  { "bua", NULL, NULL , "Buriat" },
  { "bug", NULL, NULL , "Buginese" },
  { "bul", "bg", NULL , "Bulgarian" },
  { "bur", "my", "mya", "Burmese" },
  { "byn", NULL, NULL , "Blin; Bilin" },
  { "cad", NULL, NULL , "Caddo" },
  { "cai", NULL, NULL , "Central American Indian languages" },
  { "car", NULL, NULL , "Galibi Carib" },
  { "cat", "ca", NULL , "Catalan; Valencian" },
  { "cau", NULL, NULL , "Caucasian languages" },
  { "ceb", NULL, NULL , "Cebuano" },
  { "cel", NULL, NULL , "Celtic languages" },
  { "cha", "ch", NULL , "Chamorro" },
  { "chb", NULL, NULL , "Chibcha" },
  { "che", "ce", NULL , "Chechen" },
  { "chg", NULL, NULL , "Chagatai" },
  { "chi", "zh", "zho", "Chinese" },
  { "chk", NULL, NULL , "Chuukese" },
  { "chm", NULL, NULL , "Mari" },
  { "chn", NULL, NULL , "Chinook jargon" },
  { "cho", NULL, NULL , "Choctaw" },
  { "chp", NULL, NULL , "Chipewyan; Dene Suline" },
  { "chr", NULL, NULL , "Cherokee" },
  { "chu", "cu", NULL , "Church Slavic; Old Slavonic; Church Slavonic; Old Bulgarian; Old Church Slavonic" },
  { "chv", "cv", NULL , "Chuvash" },
  { "chy", NULL, NULL , "Cheyenne" },
  { "cmc", NULL, NULL , "Chamic languages" },
  { "cop", NULL, NULL , "Coptic" },
  { "cor", "kw", NULL , "Cornish" },
  { "cos", "co", NULL , "Corsican" },
  { "cre", "cr", NULL , "Cree" },
  { "crh", NULL, NULL , "Crimean Tatar; Crimean Turkish" },
  { "crp", NULL, NULL , "Creoles and pidgins" },
  { "csb", NULL, NULL , "Kashubian" },
  { "cus", NULL, NULL , "Cushitic languages" },
  { "cze", "cs", "ces", "Czech" },
  { "dak", NULL, NULL , "Dakota" },
  { "dan", "da", NULL , "Danish" },
  { "dar", NULL, NULL , "Dargwa" },
  { "day", NULL, NULL , "Land Dayak languages" },
  { "del", NULL, NULL , "Delaware" },
  { "den", NULL, NULL , "Slave (Athapascan)" },
  { "dgr", NULL, NULL , "Dogrib" },
  { "din", NULL, NULL , "Dinka" },
  { "div", "dv", NULL , "Divehi; Dhivehi; Maldivian" },
  { "doi", NULL, NULL , "Dogri" },
  { "dra", NULL, NULL , "Dravidian languages" },
  { "dsb", NULL, NULL , "Lower Sorbian" },
  { "dua", NULL, NULL , "Duala" },
  { "dut", "nl", "nld", "Dutch; Flemish" },
  { "dyu", NULL, NULL , "Dyula" },
  { "dzo", "dz", NULL , "Dzongkha" },
  { "efi", NULL, NULL , "Efik" },
  { "egy", NULL, NULL , "Egyptian (Ancient)" },
  { "eka", NULL, NULL , "Ekajuk" },
  { "elx", NULL, NULL , "Elamite" },
  { "eng", "en", NULL , "English", "US|GB" },
  { "epo", "eo", NULL , "Esperanto" },
  { "est", "et", NULL , "Estonian" },
  { "ewe", "ee", NULL , "Ewe" },
  { "ewo", NULL, NULL , "Ewondo" },
  { "fan", NULL, NULL , "Fang" },
  { "fao", "fo", NULL , "Faroese" },
  { "fat", NULL, NULL , "Fanti" },
  { "fij", "fj", NULL , "Fijian" },
  { "fil", NULL, NULL , "Filipino; Pilipino" },
  { "fin", "fi", NULL , "Finnish" },
  { "fiu", NULL, NULL , "Finno-Ugrian languages" },
  { "fon", NULL, NULL , "Fon" },
  { "fre", "fr", "fra", "French" },
  { "frr", NULL, NULL , "Northern Frisian" },
  { "frs", NULL, NULL , "Eastern Frisian" },
  { "fry", "fy", NULL , "Western Frisian" },
  { "ful", "ff", NULL , "Fulah" },
  { "fur", NULL, NULL , "Friulian" },
  { "gaa", NULL, NULL , "Ga" },
  { "gay", NULL, NULL , "Gayo" },
  { "gba", NULL, NULL , "Gbaya" },
  { "gem", NULL, NULL , "Germanic languages" },
  { "geo", "ka", "kat", "Georgian" },
  { "ger", "de", "deu", "German" },
  { "gez", NULL, NULL , "Geez" },
  { "gil", NULL, NULL , "Gilbertese" },
  { "gla", "gd", NULL , "Gaelic; Scottish Gaelic" },
  { "gle", "ga", NULL , "Irish" },
  { "glg", "gl", NULL , "Galician" },
  { "glv", "gv", NULL , "Manx" },
  { "gon", NULL, NULL , "Gondi" },
  { "gor", NULL, NULL , "Gorontalo" },
  { "got", NULL, NULL , "Gothic" },
  { "grb", NULL, NULL , "Grebo" },
  { "gre", "el", NULL , "Greek" },
  { "grn", "gn", NULL , "Guarani" },
  { "gsw", NULL, NULL , "Swiss German; Alemannic; Alsatian" },
  { "guj", "gu", NULL , "Gujarati" },
  { "gwi", NULL, NULL , "Gwich'in" },
  { "hai", NULL, NULL , "Haida" },
  { "hat", "ht", NULL , "Haitian; Haitian Creole" },
  { "hau", "ha", NULL , "Hausa" },
  { "haw", NULL, NULL , "Hawaiian" },
  { "heb", "he", NULL , "Hebrew" },
  { "her", "hz", NULL , "Herero" },
  { "hil", NULL, NULL , "Hiligaynon" },
  { "him", NULL, NULL , "Himachali languages; Western Pahari languages" },
  { "hin", "hi", NULL , "Hindi" },
  { "hit", NULL, NULL , "Hittite" },
  { "hmn", NULL, NULL , "Hmong; Mong" },
  { "hmo", "ho", NULL , "Hiri Motu" },
  { "hrv", "hr", NULL , "Croatian" },
  { "hsb", NULL, NULL , "Upper Sorbian" },
  { "hun", "hu", NULL , "Hungarian" },
  { "hup", NULL, NULL , "Hupa" },
  { "iba", NULL, NULL , "Iban" },
  { "ibo", "ig", NULL , "Igbo" },
  { "ice", "is", "isl", "Icelandic" },
  { "ido", "io", NULL , "Ido" },
  { "iii", "ii", NULL , "Sichuan Yi; Nuosu" },
  { "ijo", NULL, NULL , "Ijo languages" },
  { "iku", "iu", NULL , "Inuktitut" },
  { "ile", "ie", NULL , "Interlingue; Occidental" },
  { "ilo", NULL, NULL , "Iloko" },
  { "ina", "ia", NULL , "Interlingua (International Auxiliary Language Association)" },
  { "inc", NULL, NULL , "Indic languages" },
  { "ind", "id", NULL , "Indonesian" },
  { "ine", NULL, NULL , "Indo-European languages" },
  { "inh", NULL, NULL , "Ingush" },
  { "ipk", "ik", NULL , "Inupiaq" },
  { "ira", NULL, NULL , "Iranian languages" },
  { "iro", NULL, NULL , "Iroquoian languages" },
  { "ita", "it", NULL , "Italian" },
  { "jav", "jv", NULL , "Javanese" },
  { "jbo", NULL, NULL , "Lojban" },
  { "jpn", "ja", NULL , "Japanese" },
  { "jpr", NULL, NULL , "Judeo-Persian" },
  { "jrb", NULL, NULL , "Judeo-Arabic" },
  { "kaa", NULL, NULL , "Kara-Kalpak" },
  { "kab", NULL, NULL , "Kabyle" },
  { "kac", NULL, NULL , "Kachin; Jingpho" },
  { "kal", "kl", NULL , "Kalaallisut; Greenlandic" },
  { "kam", NULL, NULL , "Kamba" },
  { "kan", "kn", NULL , "Kannada" },
  { "kar", NULL, NULL , "Karen languages" },
  { "kas", "ks", NULL , "Kashmiri" },
  { "kau", "kr", NULL , "Kanuri" },
  { "kaw", NULL, NULL , "Kawi" },
  { "kaz", "kk", NULL , "Kazakh" },
  { "kbd", NULL, NULL , "Kabardian" },
  { "kha", NULL, NULL , "Khasi" },
  { "khi", NULL, NULL , "Khoisan languages" },
  { "khm", "km", NULL , "Central Khmer" },
  { "kho", NULL, NULL , "Khotanese; Sakan" },
  { "kik", "ki", NULL , "Kikuyu; Gikuyu" },
  { "kin", "rw", NULL , "Kinyarwanda" },
  { "kir", "ky", NULL , "Kirghiz; Kyrgyz" },
  { "kmb", NULL, NULL , "Kimbundu" },
  { "kok", NULL, NULL , "Konkani" },
  { "kom", "kv", NULL , "Komi" },
  { "kon", "kg", NULL , "Kongo" },
  { "kor", "ko", NULL , "Korean" },
  { "kos", NULL, NULL , "Kosraean" },
  { "kpe", NULL, NULL , "Kpelle" },
  { "krc", NULL, NULL , "Karachay-Balkar" },
  { "krl", NULL, NULL , "Karelian" },
  { "kro", NULL, NULL , "Kru languages" },
  { "kru", NULL, NULL , "Kurukh" },
  { "kua", "kj", NULL , "Kuanyama; Kwanyama" },
  { "kum", NULL, NULL , "Kumyk" },
  { "kur", "ku", NULL , "Kurdish" },
  { "kut", NULL, NULL , "Kutenai" },
  { "lad", NULL, NULL , "Ladino" },
  { "lah", NULL, NULL , "Lahnda" },
  { "lam", NULL, NULL , "Lamba" },
  { "lao", "lo", NULL , "Lao" },
  { "lat", "la", NULL , "Latin" },
  { "lav", "lv", NULL , "Latvian" },
  { "lez", NULL, NULL , "Lezghian" },
  { "lim", "li", NULL , "Limburgan; Limburger; Limburgish" },
  { "lin", "ln", NULL , "Lingala" },
  { "lit", "lt", NULL , "Lithuanian" },
  { "lol", NULL, NULL , "Mongo" },
  { "loz", NULL, NULL , "Lozi" },
  { "ltz", "lb", NULL , "Luxembourgish; Letzeburgesch" },
  { "lua", NULL, NULL , "Luba-Lulua" },
  { "lub", "lu", NULL , "Luba-Katanga" },
  { "lug", "lg", NULL , "Ganda" },
  { "lui", NULL, NULL , "Luiseno" },
  { "lun", NULL, NULL , "Lunda" },
  { "luo", NULL, NULL , "Luo (Kenya and Tanzania)" },
  { "lus", NULL, NULL , "Lushai" },
  { "mac", "mk", "mkd", "Macedonian" },
  { "mad", NULL, NULL , "Madurese" },
  { "mag", NULL, NULL , "Magahi" },
  { "mah", "mh", NULL , "Marshallese" },
  { "mai", NULL, NULL , "Maithili" },
  { "mak", NULL, NULL , "Makasar" },
  { "mal", "ml", NULL , "Malayalam" },
  { "man", NULL, NULL , "Mandingo" },
  { "mao", "mi", "mri", "Maori" },
  { "map", NULL, NULL , "Austronesian languages" },
  { "mar", "mr", NULL , "Marathi" },
  { "mas", NULL, NULL , "Masai" },
  { "may", "ms", "msa", "Malay" },
  { "mdf", NULL, NULL , "Moksha" },
  { "mdr", NULL, NULL , "Mandar" },
  { "men", NULL, NULL , "Mende" },
  { "mic", NULL, NULL , "Mi'kmaq; Micmac" },
  { "min", NULL, NULL , "Minangkabau" },
  { "mis", NULL, NULL , "Uncoded languages" },
  { "mkh", NULL, NULL , "Mon-Khmer languages" },
  { "mlg", "mg", NULL , "Malagasy" },
  { "mlt", "mt", NULL , "Maltese" },
  { "mnc", NULL, NULL , "Manchu" },
  { "mni", NULL, NULL , "Manipuri" },
  { "mno", NULL, NULL , "Manobo languages" },
  { "moh", NULL, NULL , "Mohawk" },
  { "mon", "mn", NULL , "Mongolian" },
  { "mos", NULL, NULL , "Mossi" },
  { "mul", NULL, NULL , "Multiple languages" },
  { "mun", NULL, NULL , "Munda languages" },
  { "mus", NULL, NULL , "Creek" },
  { "mwl", NULL, NULL , "Mirandese" },
  { "mwr", NULL, NULL , "Marwari" },
  { "myn", NULL, NULL , "Mayan languages" },
  { "myv", NULL, NULL , "Erzya" },
  { "nah", NULL, NULL , "Nahuatl languages" },
  { "nai", NULL, NULL , "North American Indian languages" },
  { "nap", NULL, NULL , "Neapolitan" },
  { "nar", NULL, NULL , "Narration: (audio described)"},
  // Note: above is not part of the ISO spec, but is used in DVB
  { "nau", "na", NULL , "Nauru" },
  { "nav", "nv", NULL , "Navajo; Navaho" },
  { "ndo", "ng", NULL , "Ndonga" },
  { "nep", "ne", NULL , "Nepali" },
  { "new", NULL, NULL , "Nepal Bhasa; Newari" },
  { "nia", NULL, NULL , "Nias" },
  { "nic", NULL, NULL , "Niger-Kordofanian languages" },
  { "niu", NULL, NULL , "Niuean" },
  { "nog", NULL, NULL , "Nogai" },
  { "nor", "no", NULL , "Norwegian" },
  { "nqo", NULL, NULL , "N'Ko" },
  { "nso", NULL, NULL , "Pedi; Sepedi; Northern Sotho" },
  { "nub", NULL, NULL , "Nubian languages" },
  { "nwc", NULL, NULL , "Classical Newari; Old Newari; Classical Nepal Bhasa" },
  { "nya", "ny", NULL , "Chichewa; Chewa; Nyanja" },
  { "nym", NULL, NULL , "Nyamwezi" },
  { "nyn", NULL, NULL , "Nyankole" },
  { "nyo", NULL, NULL , "Nyoro" },
  { "nzi", NULL, NULL , "Nzima" },
  { "oci", "oc", NULL , "Occitan (post 1500)" },
  { "oji", "oj", NULL , "Ojibwa" },
  { "ori", "or", NULL , "Oriya" },
  { "orm", "om", NULL , "Oromo" },
  { "osa", NULL, NULL , "Osage" },
  { "oss", "os", NULL , "Ossetian; Ossetic" },
  { "oto", NULL, NULL , "Otomian languages" },
  { "paa", NULL, NULL , "Papuan languages" },
  { "pag", NULL, NULL , "Pangasinan" },
  { "pal", NULL, NULL , "Pahlavi" },
  { "pam", NULL, NULL , "Pampanga; Kapampangan" },
  { "pan", "pa", NULL , "Panjabi; Punjabi" },
  { "pap", NULL, NULL , "Papiamento" },
  { "pau", NULL, NULL , "Palauan" },
  { "per", "fa", "fas", "Persian" },
  { "phi", NULL, NULL , "Philippine languages" },
  { "phn", NULL, NULL , "Phoenician" },
  { "pli", "pi", NULL , "Pali" },
  { "pol", "pl", NULL , "Polish" },
  { "pon", NULL, NULL , "Pohnpeian" },
  { "por", "pt", NULL , "Portuguese" },
  { "pra", NULL, NULL , "Prakrit languages" },
  { "pus", "ps", NULL , "Pushto; Pashto" },
  { "qaa", NULL, NULL , "Reserved" },
  // Note: above is actually range from qaa to qtz
  { "que", "qu", NULL , "Quechua" },
  { "raj", NULL, NULL , "Rajasthani" },
  { "rap", NULL, NULL , "Rapanui" },
  { "rar", NULL, NULL , "Rarotongan; Cook Islands Maori" },
  { "roa", NULL, NULL , "Romance languages" },
  { "roh", "rm", NULL , "Romansh" },
  { "rom", NULL, NULL , "Romany" },
  { "rum", "ro", "ron", "Romanian; Moldavian; Moldovan" },
  { "run", "rn", NULL , "Rundi" },
  { "rup", NULL, NULL , "Aromanian; Arumanian; Macedo-Romanian" },
  { "rus", "ru", NULL , "Russian" },
  { "sad", NULL, NULL , "Sandawe" },
  { "sag", "sg", NULL , "Sango" },
  { "sah", NULL, NULL , "Yakut" },
  { "sai", NULL, NULL , "South American Indian languages" },
  { "sal", NULL, NULL , "Salishan languages" },
  { "sam", NULL, NULL , "Samaritan Aramaic" },
  { "san", "sa", NULL , "Sanskrit" },
  { "sas", NULL, NULL , "Sasak" },
  { "sat", NULL, NULL , "Santali" },
  { "scn", NULL, NULL , "Sicilian" },
  { "sco", NULL, NULL , "Scots" },
  { "sel", NULL, NULL , "Selkup" },
  { "sem", NULL, NULL , "Semitic languages" },
  { "sgn", NULL, NULL , "Sign Languages" },
  { "shn", NULL, NULL , "Shan" },
  { "sid", NULL, NULL , "Sidamo" },
  { "sin", "si", NULL , "Sinhala; Sinhalese" },
  { "sio", NULL, NULL , "Siouan languages" },
  { "sit", NULL, NULL , "Sino-Tibetan languages" },
  { "sla", NULL, NULL , "Slavic languages" },
  { "slo", "sk", "slk", "Slovak" },
  { "slv", "sl", NULL , "Slovenian" },
  { "sma", NULL, NULL , "Southern Sami" },
  { "sme", "se", NULL , "Northern Sami" },
  { "smi", NULL, NULL , "Sami languages" },
  { "smj", NULL, NULL , "Lule Sami" },
  { "smn", NULL, NULL , "Inari Sami" },
  { "smo", "sm", NULL , "Samoan" },
  { "sms", NULL, NULL , "Skolt Sami" },
  { "sna", "sn", NULL , "Shona" },
  { "snd", "sd", NULL , "Sindhi" },
  { "snk", NULL, NULL , "Soninke" },
  { "sog", NULL, NULL , "Sogdian" },
  { "som", "so", NULL , "Somali" },
  { "son", NULL, NULL , "Songhai languages" },
  { "spa", "es", NULL , "Spanish; Castilian" },
  { "srd", "sc", NULL , "Sardinian" },
  { "srn", NULL, NULL , "Sranan Tongo" },
  { "srp", "sr", NULL , "Serbian" },
  { "srr", NULL, NULL , "Serer" },
  { "ssa", NULL, NULL , "Nilo-Saharan languages" },
  { "ssw", "ss", NULL , "Swati" },
  { "suk", NULL, NULL , "Sukuma" },
  { "sun", "su", NULL , "Sundanese" },
  { "sus", NULL, NULL , "Susu" },
  { "sux", NULL, NULL , "Sumerian" },
  { "swa", "sw", NULL , "Swahili" },
  { "swe", "sv", NULL , "Swedish" },
  { "syc", NULL, NULL , "Classical Syriac" },
  { "syn", NULL, NULL , "Narration: (sync audio described)"},
  { "syr", NULL, NULL , "Syriac" },
  { "tah", "ty", NULL , "Tahitian" },
  { "tai", NULL, NULL , "Tai languages" },
  { "tam", "ta", NULL , "Tamil" },
  { "tat", "tt", NULL , "Tatar" },
  { "tel", "te", NULL , "Telugu" },
  { "tem", NULL, NULL , "Timne" },
  { "ter", NULL, NULL , "Tereno" },
  { "tet", NULL, NULL , "Tetum" },
  { "tgk", "tg", NULL , "Tajik" },
  { "tgl", "tl", NULL , "Tagalog" },
  { "tha", "th", NULL , "Thai" },
  { "tib", "bo", "bod", "Tibetan" },
  { "tig", NULL, NULL , "Tigre" },
  { "tir", "ti", NULL , "Tigrinya" },
  { "tiv", NULL, NULL , "Tiv" },
  { "tkl", NULL, NULL , "Tokelau" },
  { "tlh", NULL, NULL , "Klingon; tlhIngan-Hol" },
  { "tli", NULL, NULL , "Tlingit" },
  { "tmh", NULL, NULL , "Tamashek" },
  { "tog", NULL, NULL , "Tonga (Nyasa)" },
  { "ton", "to", NULL , "Tonga (Tonga Islands)" },
  { "tpi", NULL, NULL , "Tok Pisin" },
  { "tsi", NULL, NULL , "Tsimshian" },
  { "tsn", "tn", NULL , "Tswana" },
  { "tso", "ts", NULL , "Tsonga" },
  { "tuk", "tk", NULL , "Turkmen" },
  { "tum", NULL, NULL , "Tumbuka" },
  { "tup", NULL, NULL , "Tupi languages" },
  { "tur", "tr", NULL , "Turkish" },
  { "tut", NULL, NULL , "Altaic languages" },
  { "tvl", NULL, NULL , "Tuvalu" },
  { "twi", "tw", NULL , "Twi" },
  { "tyv", NULL, NULL , "Tuvinian" },
  { "udm", NULL, NULL , "Udmurt" },
  { "uga", NULL, NULL , "Ugaritic" },
  { "uig", "ug", NULL , "Uighur; Uyghur" },
  { "ukr", "uk", NULL , "Ukrainian" },
  { "umb", NULL, NULL , "Umbundu" },
  { "urd", "ur", NULL , "Urdu" },
  { "uzb", "uz", NULL , "Uzbek" },
  { "v.o", NULL, NULL , "Voice Original" },
  { "vai", NULL, NULL , "Vai" },
  { "ven", "ve", NULL , "Venda" },
  { "vie", "vi", NULL , "Vietnamese" },
  { "vol", "vo", NULL , "Volapük" },
  { "vot", NULL, NULL , "Votic" },
  { "wak", NULL, NULL , "Wakashan languages" },
  { "wal", NULL, NULL , "Wolaitta; Wolaytta" },
  { "war", NULL, NULL , "Waray" },
  { "was", NULL, NULL , "Washo" },
  { "wel", "cy", "cym", "Welsh" },
  { "wen", NULL, NULL , "Sorbian languages" },
  { "wln", "wa", NULL , "Walloon" },
  { "wol", "wo", NULL , "Wolof" },
  { "xal", NULL, NULL , "Kalmyk; Oirat" },
  { "xho", "xh", NULL , "Xhosa" },
  { "yao", NULL, NULL , "Yao" },
  { "yap", NULL, NULL , "Yapese" },
  { "yid", "yi", NULL , "Yiddish" },
  { "yor", "yo", NULL , "Yoruba" },
  { "ypk", NULL, NULL , "Yupik languages" },
  { "zap", NULL, NULL , "Zapotec" },
  { "zbl", NULL, NULL , "Blissymbols; Blissymbolics; Bliss" },
  { "zen", NULL, NULL , "Zenaga" },
  { "zha", "za", NULL , "Zhuang; Chuang" },
  { "znd", NULL, NULL , "Zande languages" },
  { "zul", "zu", NULL , "Zulu" },
  { "zun", NULL, NULL , "Zuni" },
  { "zza", NULL, NULL , "Zaza; Dimili; Dimli; Kirdki; Kirmanjki; Zazaki" },
  { NULL, NULL, NULL, NULL }
};

lang_code_lookup_t* lang_codes_code2b = NULL;
lang_code_lookup_t* lang_codes_code1 = NULL;
lang_code_lookup_t* lang_codes_code2t = NULL;

/* **************************************************************************
 * Functions
 * *************************************************************************/

/* Compare language codes */
static int _lang_code2b_cmp ( void *a, void *b )
{
  return strcmp(((lang_code_lookup_element_t*)a)->lang_code->code2b,
      ((lang_code_lookup_element_t*)b)->lang_code->code2b);
}

static int _lang_code1_cmp ( void *a, void *b )
{
  return strcmp(((lang_code_lookup_element_t*)a)->lang_code->code1,
      ((lang_code_lookup_element_t*)b)->lang_code->code1);
}

static int _lang_code2t_cmp ( void *a, void *b )
{
  return strcmp(((lang_code_lookup_element_t*)a)->lang_code->code2t,
      ((lang_code_lookup_element_t*)b)->lang_code->code2t);
}

static int _lang_code_lookup_add( lang_code_lookup_t* lookup_table, const lang_code_t *code, int (*func)(void*, void*) ) {
  lang_code_lookup_element_t *element;
  element = (lang_code_lookup_element_t *)calloc(1, sizeof(lang_code_lookup_element_t));
  element->lang_code = code;
  RB_INSERT_SORTED(lookup_table, element, link, func);
  return 0;
}

static const lang_code_t *_lang_code_get ( const char *code, size_t len )
{
  int i;
  char tmp[4];
  
  if (lang_codes_code2b == NULL) {
    const lang_code_t *c = lang_codes;

    lang_codes_code2b = (lang_code_lookup_t *)calloc(1, sizeof(lang_code_lookup_t));
    lang_codes_code1 = (lang_code_lookup_t *)calloc(1, sizeof(lang_code_lookup_t));
    lang_codes_code2t = (lang_code_lookup_t *)calloc(1, sizeof(lang_code_lookup_t));

    while (c->code2b) {
      _lang_code_lookup_add(lang_codes_code2b, c, _lang_code2b_cmp);
      if (c->code1) _lang_code_lookup_add(lang_codes_code1, c, _lang_code1_cmp);
      if (c->code2t) _lang_code_lookup_add(lang_codes_code2t, c, _lang_code2t_cmp);
      c++;
    }
  }

  if (code && *code && len) {

    /* Extract the code (lowercase) */
    i = 0;
    while (i < 3 && *code && len) {
      if (*code == ';' || *code == ',' || *code == '-') break;
      if (*code != ' ')
        tmp[i++] = *code | 0x20; // |0x20 = lower case
      code++;
      len--;
    }
    tmp[i] = '\0';

    /* Convert special case (qaa..qtz) */
    if (*tmp == 'q') {
      if (tmp[1] >= 'a' && tmp[1] <= 'z' && tmp[2] >= 'a' && tmp[2] <= 'z') {
        tmp[1] = 'a';
        tmp[2] = 'a';
      }
    }

    /* Search */
    if (i) {
      lang_code_lookup_element_t sample, *element;
      lang_code_t lang_code;
      lang_code.code1 = tmp;
      lang_code.code2b = tmp;
      lang_code.code2t = tmp;
      sample.lang_code = &lang_code;
      element = RB_FIND(lang_codes_code2b, &sample, link, _lang_code2b_cmp);
      if (element != NULL) return element->lang_code;
      element = RB_FIND(lang_codes_code1, &sample, link, _lang_code1_cmp);
      if (element != NULL) return element->lang_code;
      element = RB_FIND(lang_codes_code2t, &sample, link, _lang_code2t_cmp);
      if (element != NULL) return element->lang_code;
    }
  }
  return &lang_codes[0];
}

const char *lang_code_get ( const char *code )
{
  return lang_code_get3(code)->code2b;
}

const char *lang_code_get2 ( const char *code, size_t len )
{
  return _lang_code_get(code, len)->code2b;
}

const lang_code_t *lang_code_get3 ( const char *code )
{
  return _lang_code_get(code, strlen(code ?: ""));
}

const char **lang_code_split ( const char *codes )
{
  int i = 0;
  const lang_code_t **lcs = lang_code_split2(codes);
  const char **ret;

  if(!lcs) return NULL;

  while(lcs[i])
    i++;

  ret = calloc(1+i, sizeof(char*));

  i = 0;
  while(lcs[i]) {
      ret[i] = lcs[i]->code2b;
      i++;
  }
  ret[i] = NULL;
  free(lcs);

  return ret;
}

const lang_code_t **lang_code_split2 ( const char *codes )
{
  int n;
  const char *c, *p;
  const lang_code_t **ret;
  const lang_code_t *co;

  /* Defaults */
  if (!codes) codes = config_get_language();

  /* No config */
  if (!codes) return NULL;

  /* Count entries */
  n = 0;
  c = codes;
  while (*c) {
    if (*c == ',') n++;
    c++;
  }
  ret = calloc(2+n, sizeof(lang_code_t*));

  /* Create list */
  n = 0;
  p = c = codes;
  while (*c) {
    if (*c == ',') {
      co = lang_code_get3(p);
      if(co)
        ret[n++] = co;
      p = c + 1;
    }
    c++;
  }
  if (*p) ret[n++] = lang_code_get3(p);
  ret[n] = NULL;

  return ret;
}

static void lang_code_free( lang_code_lookup_t *l )
{
  lang_code_lookup_element_t *element;
  if (l == NULL)
    return;
  while ((element = RB_FIRST(l)) != NULL) {
    RB_REMOVE(l, element, link);
    free(element);
  }
  free(l);
}

const char *lang_code_preferred( void )
{
  const char *codes = config_get_language(), *ret = "und";
  const lang_code_t *co;

  if (codes) {
    co = lang_code_get3(codes);
    if (co && co->code2b)
      ret = co->code2b;
  }

  return ret;
}

char *lang_code_user( const char *ucode )
{
  const char *codes = config_get_language(), *s;
  char *ret;

  if (!codes)
    return ucode ? strdup(ucode) : NULL;
  if (!ucode)
    return codes ? strdup(codes) : NULL;
  ret = malloc(strlen(codes) + strlen(ucode ?: "") + 1);
  strcpy(ret, ucode);
  while (codes && *codes) {
    s = codes;
    while (*s && *s != ',')
      s++;
    if (strncmp(codes, ucode ?: "", s - codes)) {
      strcat(ret, ",");
      strncat(ret, codes, s - codes);
    }
    if (*s && *s == ',')
      s++;
    codes = s;
  }
  return ret;
}

void lang_code_done( void )
{
  lang_code_free(lang_codes_code2b);
  lang_code_free(lang_codes_code1);
  lang_code_free(lang_codes_code2t);
}
