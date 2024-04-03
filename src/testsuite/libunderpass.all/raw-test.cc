//
// Copyright (c) 2024 Humanitarian OpenStreetMap Team
//
// This file is part of Underpass.
//
//     Underpass is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     Underpass is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with Underpass.  If not, see <https://www.gnu.org/licenses/>.
//

#include <dejagnu.h>
#include <iostream>
#include "utils/log.hh"
#include "osm/osmobjects.hh"
#include "raw/queryraw.hh"
#include <string.h>
#include "replicator/replication.hh"
#include <boost/geometry.hpp>

using namespace replication;
using namespace logger;
using namespace queryraw;

TestState runtest;
class TestOsmChange : public osmchange::OsmChangeFile {};

class TestPlanet : public Planet {
  public:
    TestPlanet() = default;

    //! Clear the test DB and fill it with with initial test data
    bool init_test_case(const std::string &dbconn)
    {
        source_tree_root = getenv("UNDERPASS_SOURCE_TREE_ROOT")
                               ? getenv("UNDERPASS_SOURCE_TREE_ROOT")
                               : "../";

        const std::string test_replication_db_name{"underpass_test"};
        {
            pqxx::connection conn{dbconn + " dbname=template1"};
            pqxx::nontransaction worker{conn};
            worker.exec0("DROP DATABASE IF EXISTS " + test_replication_db_name);
            worker.exec0("CREATE DATABASE " + test_replication_db_name);
            worker.commit();
        }

        {
            pqxx::connection conn{dbconn + " dbname=" + test_replication_db_name};
            pqxx::nontransaction worker{conn};
            worker.exec0("CREATE EXTENSION postgis");
            worker.exec0("CREATE EXTENSION hstore");

            // Create schema
            const auto schema_path{source_tree_root + "setup/db/underpass.sql"};
            std::ifstream schema_definition(schema_path);
            std::string sql((std::istreambuf_iterator<char>(schema_definition)),
                            std::istreambuf_iterator<char>());

            assert(!sql.empty());
            worker.exec0(sql);
        }

        return true;
    };

    std::string source_tree_root;
};

bool processFile(const std::string &filename, std::shared_ptr<Pq> &db) {
    auto queryraw = std::make_shared<QueryRaw>(db);
    auto osmchanges = std::make_shared<osmchange::OsmChangeFile>();
    std::string destdir_base = DATADIR;
    multipolygon_t poly;
    osmchanges->readChanges(destdir_base + "/testsuite/testdata/raw/" + filename);
    queryraw->buildGeometries(osmchanges, poly);
    osmchanges->areaFilter(poly);
    std::string rawquery;

    for (auto it = std::begin(osmchanges->changes); it != std::end(osmchanges->changes); ++it) {
        osmchange::OsmChange *change = it->get();
        // Nodes
        for (auto nit = std::begin(change->nodes); nit != std::end(change->nodes); ++nit) {
            osmobjects::OsmNode *node = nit->get();
            rawquery += queryraw->applyChange(*node);
        }
        // Ways
        for (auto wit = std::begin(change->ways); wit != std::end(change->ways); ++wit) {
            osmobjects::OsmWay *way = wit->get();
            rawquery += queryraw->applyChange(*way);
        }
        // Relations
        for (auto rit = std::begin(change->relations); rit != std::end(change->relations); ++rit) {
            osmobjects::OsmRelation *relation = rit->get();
            rawquery += queryraw->applyChange(*relation);
        }
    }
    db->query(rawquery);
}

const std::vector<std::string> expectedGeometries = {
    "POLYGON((21.7260014 4.6204295,21.7260865 4.6204274,21.7260849 4.6203649,21.7259998 4.620367,21.7260014 4.6204295))",
    "POLYGON((21.7260014 4.6204295,21.7260865 4.6204274,21.7260849 4.6203649,21.7259998 4.620367,21.7260014 4.6204295))",
    "POLYGON((21.7260114 4.6204395,21.7260865 4.6204274,21.7260849 4.6203649,21.7259998 4.620367,21.7260114 4.6204395))",
    "MULTIPOLYGON(((21.7260114 4.6204395,21.7260865 4.6204274,21.7260849 4.6203649,21.7259998 4.620367,21.7260114 4.6204395),(21.726017 4.6204134,21.72607138 4.6204132,21.72607088 4.6203768,21.72601656 4.6203803,21.726017 4.6204134)))",
    "MULTIPOLYGON(((21.7260114 4.6204395,21.7260865 4.6204274,21.7260807 4.6203703,21.7259998 4.620367,21.7260114 4.6204395),(21.726017 4.6204134,21.72607138 4.6204132,21.72607088 4.6203768,21.72601656 4.6203803,21.726017 4.6204134)))",
    "MULTILINESTRING((21.7260849 4.6203649,21.7260865 4.6204274,21.7260014 4.6204295,21.7260014 4.6204295,21.7259998 4.620367,21.7260849 4.6203649))",
    "MULTIPOLYGON(((-59.8355946 -36.7448782,-59.8353203 -36.7452085,-59.8368532 -36.7480926,-59.8370667 -36.7483355,-59.8408639 -36.7515154,-59.8482593 -36.745907,-59.8482593 -36.745907,-59.8483958 -36.7456167,-59.848532 -36.7453269,-59.8486299 -36.7452032,-59.8486299 -36.7452032,-59.8486213 -36.7451521,-59.8481455 -36.7432981,-59.8478834 -36.7425679,-59.8478326 -36.7423963,-59.8478326 -36.7423963,-59.8477902 -36.7422529,-59.8477764 -36.7422007,-59.8477733 -36.74215,-59.8477773 -36.742095,-59.8477972 -36.7420329,-59.8478247 -36.7419783,-59.8478609 -36.7419236,-59.8479126 -36.7418808,-59.84795 -36.7418607,-59.8480462 -36.7418147,-59.8480692 -36.7417819,-59.8480709 -36.7417548,-59.8480673 -36.7416893,-59.8479835 -36.741163,-59.8479787 -36.7411211,-59.8479562 -36.7410819,-59.847923 -36.7410414,-59.8478722 -36.7410094,-59.8475273 -36.7408127,-59.8474352 -36.7407496,-59.8473715 -36.7406912,-59.8473254 -36.7406329,-59.8472829 -36.7405588,-59.8472494 -36.7404962,-59.8472589 -36.7404251,-59.8472978 -36.7403582,-59.8473424 -36.74032,-59.8473864 -36.7402921,-59.8474375 -36.740274,-59.8474922 -36.740256,-59.8475468 -36.7402465,-59.8476241 -36.7402304,-59.8476784 -36.7402171,-59.8477563 -36.7401919,-59.8478265 -36.7401545,-59.8479128 -36.7400779,-59.8479937 -36.7399885,-59.8480801 -36.7398796,-59.8481504 -36.7397665,-59.848411 -36.7391216,-59.8484023 -36.739028,-59.8483557 -36.7389343,-59.8482783 -36.7388702,-59.84822 -36.7388242,-59.8481308 -36.7388079,-59.8480003 -36.7387583,-59.8478575 -36.7386841,-59.8477933 -36.738618,-59.8477365 -36.7385158,-59.8477106 -36.7384235,-59.8477053 -36.7382963,-59.8477134 -36.7381998,-59.8477433 -36.7381126,-59.8478321 -36.738022,-59.8479105 -36.7379644,-59.8480011 -36.7379216,-59.8482127 -36.7378122,-59.8482877 -36.7377698,-59.8483566 -36.7377126,-59.848393 -36.737632,-59.8484294 -36.7375366,-59.8485761 -36.7372026,-59.848605 -36.7370773,-59.8486246 -36.7369372,-59.8486196 -36.7368366,-59.8485807 -36.7367015,-59.848529 -36.7365836,-59.8484717 -36.7364835,-59.8483887 -36.7363497,-59.8477548 -36.7356502,-59.8477339 -36.7356248,-59.8477339 -36.7356248,-59.8475634 -36.7357007,-59.8474292 -36.7357691,-59.8473073 -36.7358571,-59.8469617 -36.7361243,-59.8447338 -36.737825,-59.8424572 -36.7395354,-59.8423067 -36.7396527,-59.8386641 -36.7424968,-59.838225 -36.7428388,-59.8355946 -36.7448782)))",
    "MULTIPOLYGON(((-69.0305328 -33.683019,-69.0311444 -33.6821173,-69.0274215 -33.6803451,-69.0263647 -33.6819119,-69.0305328 -33.683019),(-69.0344971 -33.6875005,-69.0354574 -33.6880228,-69.0356076 -33.6879112,-69.0360421 -33.6881656,-69.0362352 -33.6883218,-69.0369111 -33.6886075,-69.0375173 -33.6871078,-69.0367931 -33.686581,-69.0366161 -33.6866525,-69.0361923 -33.6865766,-69.0364122 -33.6860945,-69.0368253 -33.68634,-69.0370399 -33.6864739,-69.037512 -33.6861168,-69.0374959 -33.6859115,-69.0376568 -33.6857687,-69.037866 -33.6856838,-69.037351 -33.6853401,-69.0371311 -33.6842242,-69.0365088 -33.68376,-69.0362889 -33.6832065,-69.036144 -33.6823673,-69.0358865 -33.6818227,-69.0358973 -33.6817468,-69.03557 -33.6815816,-69.0359187 -33.6810459,-69.0351462 -33.6804031,-69.0349263 -33.6798541,-69.0345454 -33.67968,-69.0342611 -33.6794791,-69.0337515 -33.6794836,-69.0332151 -33.6793095,-69.0331185 -33.6788586,-69.0329737 -33.6786354,-69.0327162 -33.6783541,-69.0325767 -33.6782024,-69.0321851 -33.6780238,-69.0315521 -33.6776756,-69.0312892 -33.6774881,-69.0311068 -33.6773453,-69.0308118 -33.6771131,-69.0305275 -33.6769747,-69.0303397 -33.6769033,-69.0300447 -33.676948,-69.0298086 -33.6769256,-69.0288216 -33.6784256,-69.0287519 -33.6783742,-69.0296692 -33.6768676,-69.0292937 -33.6765819,-69.0289557 -33.6765596,-69.0286982 -33.6766355,-69.0284568 -33.6765685,-69.0282691 -33.6768363,-69.0279794 -33.6769122,-69.0277594 -33.6767917,-69.0276629 -33.6770015,-69.0274376 -33.6768988,-69.0273035 -33.6768899,-69.0270782 -33.6770328,-69.0268743 -33.6769301,-69.0265229 -33.6775261,-69.0258417 -33.6786086,-69.0233284 -33.6774703,-69.0232372 -33.6775238,-69.0232211 -33.6781176,-69.0231353 -33.6783408,-69.0233445 -33.6788229,-69.0233767 -33.6791979,-69.0232158 -33.6794925,-69.0233231 -33.6796264,-69.022733 -33.6803049,-69.0227705 -33.6806933,-69.0227598 -33.6811084,-69.0223092 -33.6817245,-69.0238702 -33.6824744,-69.0240419 -33.6822735,-69.0255761 -33.6830234,-69.0254581 -33.6832243,-69.0291756 -33.6849295,-69.0293527 -33.6846617,-69.0311283 -33.6854607,-69.0308976 -33.6858222,-69.0337139 -33.6871256,-69.0343523 -33.6862061,-69.0349907 -33.6865409,-69.0343738 -33.6874604,-69.0344971 -33.6875005)))",
    "MULTIPOLYGON(((-70.3434661 -38.5210725,-70.3434387 -38.5210955,-70.3434261 -38.5211299,-70.3434276 -38.5211716,-70.3434429 -38.5212067,-70.3434724 -38.5212373,-70.3435173 -38.5212595,-70.3435951 -38.5212788,-70.3436942 -38.5212955,-70.3438252 -38.5213148,-70.3439597 -38.521337,-70.3441048 -38.5213591,-70.3442463 -38.5213841,-70.3443808 -38.5214145,-70.3445082 -38.5214533,-70.3446177 -38.5214956,-70.3447356 -38.5215433,-70.3448535 -38.5215847,-70.3449795 -38.5216101,-70.3451015 -38.5216229,-70.3452113 -38.5216419,-70.3453088 -38.5216674,-70.3454192 -38.521701,-70.3455168 -38.5217296,-70.3456184 -38.5217582,-70.3457445 -38.5217805,-70.3458786 -38.52179,-70.3460209 -38.52179,-70.3471381 -38.5217576,-70.3472051 -38.5217534,-70.3472561 -38.521743,-70.3473044 -38.5217241,-70.3473446 -38.521701,-70.3473795 -38.5216695,-70.3474116 -38.5216422,-70.3474492 -38.5216128,-70.3474921 -38.5215835,-70.3475323 -38.5215541,-70.3476101 -38.5214974,-70.3475739 -38.5216241,-70.3475456 -38.5216933,-70.3475137 -38.5217626,-70.3474677 -38.5218235,-70.3474253 -38.5218674,-70.3473686 -38.5219038,-70.3473085 -38.5219342,-70.3472413 -38.521954,-70.347174 -38.5219675,-70.3472817 -38.5219659,-70.3474005 -38.5219536,-70.3475173 -38.5219287,-70.3476447 -38.5218955,-70.3477721 -38.5218456,-70.3479455 -38.5217626,-70.3479066 -38.5218318,-70.3478641 -38.5218789,-70.3478075 -38.5219287,-70.3477355 -38.5219766,-70.3479018 -38.5219262,-70.3480467 -38.5218675,-70.3481915 -38.5217919,-70.3483042 -38.5217206,-70.3484135 -38.521645,-70.3486227 -38.5215149,-70.3488534 -38.521389,-70.3491002 -38.5212799,-70.3493684 -38.5211918,-70.3496098 -38.5211162,-70.3498834 -38.5210491,-70.3501301 -38.5210029,-70.3503447 -38.5209693,-70.3505593 -38.5209441,-70.3507792 -38.5209204,-70.3507792 -38.5209204,-70.3505605 -38.5208228,-70.350236 -38.5206906,-70.3498659 -38.5205962,-70.3495735 -38.5205458,-70.3492637 -38.5204975,-70.3491282 -38.5204933,-70.3490075 -38.5205059,-70.3488064 -38.5205469,-70.3488064 -38.5205469,-70.3485167 -38.5206098,-70.3478837 -38.5207179,-70.3473875 -38.5207808,-70.3468376 -38.5208333,-70.3461456 -38.5208816,-70.3455448 -38.520934,-70.3449628 -38.5209949,-70.3443834 -38.5210306,-70.343796 -38.5210536,-70.3434661 -38.5210725),(-70.3393449 -38.5210096,-70.3397998 -38.5209924,-70.3400091 -38.5209944,-70.3401816 -38.5210137,-70.3403726 -38.5210619,-70.340539 -38.5211246,-70.3406807 -38.5211969,-70.3408225 -38.5212789,-70.3409704 -38.5213898,-70.3410954 -38.5215145,-70.341237 -38.5216474,-70.3414069 -38.5217803,-70.3415626 -38.5218855,-70.3417466 -38.5219686,-70.3419731 -38.5220406,-70.3421855 -38.522096,-70.3423908 -38.5221458,-70.3426173 -38.5222012,-70.3428155 -38.5222621,-70.3430137 -38.5223341,-70.3432119 -38.5224338,-70.3433534 -38.5225058,-70.343495 -38.5225889,-70.3436649 -38.5226609,-70.3438772 -38.5226885,-70.3440683 -38.522683,-70.3442524 -38.5226609,-70.3449444 -38.5225329,-70.3450571 -38.5224952,-70.3451644 -38.522428,-70.3452341 -38.5223483,-70.3452663 -38.5222769,-70.3452717 -38.5222014,-70.3452448 -38.5221342,-70.3451966 -38.5220797,-70.3451 -38.5220335,-70.3449444 -38.5219873,-70.3447513 -38.5219537,-70.3445624 -38.5219413,-70.343915 -38.5219068,-70.3437005 -38.5218859,-70.3434859 -38.5218565,-70.3432337 -38.5217935,-70.3429923 -38.521718,-70.3427402 -38.5216215,-70.3425364 -38.5215333,-70.3423486 -38.5214368,-70.3421877 -38.5213361,-70.3420589 -38.5212353,-70.341948 -38.5211271,-70.341948 -38.5211271,-70.3415966 -38.5210893,-70.3412157 -38.5210327,-70.3409931 -38.5209875,-70.3408737 -38.5209414,-70.3406967 -38.5208417,-70.3405653 -38.5207452,-70.3404205 -38.5206245,-70.3402944 -38.5205217,-70.3401375 -38.5204073,-70.3399175 -38.520231,-70.3397392 -38.5200925,-70.3396024 -38.5200075,-70.3394187 -38.5199183,-70.3392309 -38.519868,-70.3388715 -38.5197882,-70.3384933 -38.5197012,-70.3380534 -38.5196015,-70.3377664 -38.519549,-70.3372943 -38.5195007,-70.3368531 -38.5194703,-70.3365098 -38.5194577,-70.3361759 -38.5194399,-70.3359184 -38.5194567,-70.3359184 -38.5194567,-70.3362585 -38.519639,-70.3365107 -38.5197779,-70.3367348 -38.5199386,-70.3369777 -38.520114,-70.3372112 -38.5202456,-70.3376501 -38.5204356,-70.3381078 -38.5206329,-70.3384721 -38.5208009,-70.3387149 -38.520874,-70.3390231 -38.5209325,-70.3393449 -38.5210096)))",
    "MULTILINESTRING((-61.7568258 -31.6740654,-61.7569584 -31.6740483,-61.7573608 -31.6739754,-61.7579382 -31.6738709,-61.758941 -31.6736955,-61.7599953 -31.6735111,-61.7603435 -31.6734503,-61.7605975 -31.6734047,-61.7621451 -31.6731352,-61.7621451 -31.6731352,-61.7621922 -31.6733343))",
    "MULTIPOLYGON(((-68.5483882 -31.5039585,-68.5480409 -31.5039305,-68.5475132 -31.503888,-68.5475069 -31.503888,-68.5473788 -31.5038875,-68.5473722 -31.5038875,-68.5472674 -31.5038814,-68.5471701 -31.5038757,-68.5470843 -31.5038707,-68.5470683 -31.5038694,-68.5469779 -31.5038617,-68.5468804 -31.5038535,-68.5467856 -31.5038454,-68.5466804 -31.5038365,-68.5465891 -31.5038288,-68.5464757 -31.5038192,-68.5465458 -31.5035879,-68.5465473 -31.5035825,-68.54675 -31.5028187,-68.5468999 -31.5022372,-68.5469021 -31.5022284,-68.5469628 -31.5019979,-68.5469813 -31.5019276,-68.5469833 -31.5019199,-68.5469942 -31.5018781,-68.5469942 -31.5018781,-68.5470365 -31.5018696,-68.5470365 -31.5018696,-68.547483 -31.5017578,-68.5477639 -31.501702,-68.5480249 -31.5016557,-68.5485587 -31.5016466,-68.5490234 -31.5016055,-68.5490234 -31.5016055,-68.5489833 -31.5018285,-68.5489533 -31.5019571,-68.5489335 -31.5020433,-68.5488177 -31.5025468,-68.5487158 -31.5030088,-68.5486036 -31.5034996,-68.5485833 -31.5035724,-68.5484826 -31.5039664,-68.5484826 -31.5039664,-68.5483946 -31.503959,-68.5483882 -31.5039585)))",
    "MULTIPOLYGON(((-68.5482663 -31.5443404,-68.5480718 -31.5442838,-68.5480346 -31.544273,-68.5479679 -31.5442535,-68.547865 -31.5442235,-68.5477976 -31.5442039,-68.547794 -31.5442039,-68.547766 -31.5442043,-68.5477546 -31.5442044,-68.547652 -31.5442057,-68.5475929 -31.5442065,-68.5475307 -31.5442073,-68.5474691 -31.5442038,-68.547375 -31.5441984,-68.5472913 -31.5441936,-68.5471354 -31.5441956,-68.5470951 -31.5442013,-68.5468837 -31.5442374,-68.5468293 -31.5442467,-68.5467335 -31.5442581,-68.5467026 -31.5442618,-68.5466636 -31.5442665,-68.5465768 -31.5442768,-68.5464498 -31.5442919,-68.5464463 -31.54439,-68.5464427 -31.54449,-68.546441 -31.544537,-68.5464384 -31.5446868,-68.5464363 -31.5447315,-68.5464364 -31.544792,-68.5464364 -31.544807,-68.5464323 -31.5449592,-68.5464319 -31.5449777,-68.5464276 -31.5451235,-68.5464275 -31.5451625,-68.5464273 -31.5451953,-68.5464272 -31.5452255,-68.5464256 -31.5453554,-68.5464243 -31.5453784,-68.5464234 -31.5454414,-68.5464189 -31.5455607,-68.5465619 -31.5455611,-68.5465918 -31.5455614,-68.5467006 -31.5455617,-68.5468407 -31.5455622,-68.5469298 -31.5455623,-68.5469764 -31.5455626,-68.5470158 -31.5455629,-68.5471183 -31.5455631,-68.5472789 -31.5455637,-68.54736 -31.545564,-68.547401 -31.5455641,-68.547446 -31.5455643,-68.5475602 -31.5455647,-68.547728 -31.5455653,-68.547817 -31.5455656,-68.5478385 -31.5455657,-68.5478385 -31.5455657,-68.5478478 -31.5455678,-68.5478478 -31.5455678,-68.5478797 -31.5454672,-68.5479144 -31.5453579,-68.5479468 -31.5452559,-68.547975 -31.5451672,-68.5480045 -31.5450741,-68.5480327 -31.5449854,-68.5480647 -31.5448846,-68.5480958 -31.5447869,-68.548114 -31.5447294,-68.5481151 -31.5447209,-68.5481097 -31.5447145,-68.5481097 -31.5447145,-68.5481476 -31.5446143,-68.5481839 -31.5445183,-68.5481839 -31.5445183,-68.5481966 -31.5445115,-68.5482761 -31.5443436,-68.5482761 -31.5443436,-68.5482663 -31.5443404)))",
    "MULTIPOLYGON(((-58.3761403 -34.8049613,-58.3746555 -34.8043325,-58.3745757 -34.8043497,-58.3741564 -34.8050218,-58.3741564 -34.8050218,-58.3741704 -34.8050491,-58.3741619 -34.8050658,-58.3741513 -34.8050827,-58.374112 -34.8050927,-58.374112 -34.8050927,-58.3737147 -34.8057548,-58.3737396 -34.8058114,-58.3751384 -34.806525,-58.3751384 -34.806525,-58.3761403 -34.8049613)))",
    "MULTILINESTRING((-65.7819384 -28.4602624,-65.78194 -28.4602013,-65.7819774 -28.4602021,-65.7819821 -28.4600267,-65.78195 -28.460026,-65.7819522 -28.4599456,-65.7819904 -28.4599464,-65.7819951 -28.4597744,-65.7819717 -28.4597739,-65.7819734 -28.459716,-65.7823023 -28.4597233,-65.7822987 -28.4598488,-65.7823247 -28.4598494,-65.7823226 -28.4599249,-65.7821596 -28.4599213,-65.7821587 -28.4599523,-65.7822528 -28.4599544,-65.7822505 -28.4600365,-65.7821389 -28.460034,-65.7821351 -28.4601647,-65.7821705 -28.4601654,-65.7821712 -28.4601403,-65.7822232 -28.4601414,-65.7822246 -28.4600897,-65.7822633 -28.4600905,-65.7822667 -28.459963,-65.7823293 -28.4599643,-65.7823258 -28.4600981,-65.7822974 -28.4600975,-65.7822929 -28.4602696,-65.7819384 -28.4602624))",
    "MULTILINESTRING((-60.3289264 -31.739336,-60.3292052 -31.7393546,-60.3294736 -31.7393725),(-60.4094365 -31.7562038,-60.4088564 -31.7561717,-60.4088554 -31.7561858,-60.4088547 -31.7561944,-60.4094348 -31.7562266,-60.4094365 -31.7562038),(-60.4490076 -31.7525825,-60.448439 -31.7526463),(-60.4546523 -31.7519354,-60.4540749 -31.7520026,-60.454079 -31.7520282,-60.4546564 -31.751961,-60.4546547 -31.7519499,-60.4546523 -31.7519354),(-60.4832498 -31.7697088,-60.4826784 -31.7696678,-60.4826772 -31.7696799,-60.4826763 -31.7696889,-60.4832477 -31.7697298,-60.4832498 -31.7697088),(-60.4955244 -31.7721043,-60.4950164 -31.7718769,-60.4950144 -31.77188,-60.4950072 -31.7718916,-60.4955153 -31.772119,-60.4955244 -31.7721043),(-60.5045121 -31.7734111,-60.5039414 -31.7733833,-60.503941 -31.7733884,-60.5039398 -31.7734072,-60.5045104 -31.773435,-60.5045121 -31.7734111),(-60.5127137 -31.7683306,-60.5127017 -31.7683237,-60.512696 -31.7683203,-60.5123721 -31.7687237,-60.5123899 -31.768734,-60.5127137 -31.7683306),(-60.5159017 -31.7643863,-60.5158879 -31.7643781,-60.5162173 -31.7639768,-60.5162208 -31.7639789,-60.516231 -31.7639849,-60.5159017 -31.7643863),(-60.5208546 -31.7580253,-60.520839 -31.7580154,-60.5208331 -31.7580117,-60.5205435 -31.758437,-60.5205649 -31.7584507,-60.5208546 -31.7580253),(-60.5331566 -31.7408199,-60.533206 -31.7408068,-60.5332684 -31.7408247,-60.533383 -31.7412668,-60.5339281 -31.7415692,-60.5309841 -31.7456282,-60.5309435 -31.7456843,-60.53044 -31.7463804,-60.5300264 -31.7461612,-60.5300312 -31.7461542,-60.5299422 -31.7461094,-60.529912 -31.7460935,-60.5298697 -31.7460714,-60.5297794 -31.7460242,-60.5297284 -31.7459975,-60.5296675 -31.7459652,-60.5296397 -31.7459394,-60.5295918 -31.7457893,-60.52956 -31.745762,-60.5293851 -31.7456733,-60.5296442 -31.7453201,-60.53048 -31.7442233,-60.53102 -31.7434927,-60.530924 -31.7434466,-60.5310456 -31.7432868,-60.5318423 -31.7426469,-60.5317027 -31.7425381,-60.5317238 -31.742426,-60.5317834 -31.7423184,-60.5322159 -31.7417072,-60.5322246 -31.741642,-60.5323013 -31.741637,-60.5324844 -31.7417341,-60.5329342 -31.7410775,-60.5331096 -31.7408541,-60.5331566 -31.7408199),(-60.3008178 -31.7269233,-60.2997508 -31.7264213,-60.2992341 -31.7262355,-60.2987165 -31.7260996,-60.2983246 -31.7260312,-60.2979304 -31.7259871,-60.2951509 -31.7257779,-60.2948654 -31.7257511),(-60.3008178 -31.7269233,-60.3014362 -31.7272238,-60.3014362 -31.7272238,-60.3024637 -31.7277236,-60.3024637 -31.7277236,-60.302867 -31.7279284,-60.302867 -31.7279284,-60.3074125 -31.7301641,-60.3074125 -31.7301641,-60.3086075 -31.7307464,-60.3086075 -31.7307464,-60.3108042 -31.7317669,-60.3127634 -31.7326402,-60.3172228 -31.7346731,-60.3253759 -31.7383794,-60.3264264 -31.7387431,-60.3274191 -31.7389943,-60.3287067 -31.739216,-60.3292215 -31.7392556,-60.330009 -31.7392862,-60.3308572 -31.7392541,-60.3317051 -31.7391853,-60.3332268 -31.7389584,-60.3375255 -31.7382145,-60.3422108 -31.7374445,-60.3432909 -31.7372748,-60.3440431 -31.7372228,-60.3445425 -31.7372449,-60.3451538 -31.737302,-60.3462993 -31.7374809,-60.3467372 -31.7375611,-60.3467372 -31.7375611,-60.3473804 -31.7376938,-60.3473804 -31.7376938,-60.3535773 -31.7390844,-60.3547467 -31.7393964,-60.3556076 -31.7396912,-60.3562877 -31.7399688,-60.3569581 -31.7403271,-60.3595767 -31.7418275,-60.3618068 -31.7431025,-60.3628968 -31.7436953,-60.3726941 -31.7491239,-60.3731728 -31.7493868,-60.3750034 -31.7503823,-60.3750034 -31.7503823,-60.3751797 -31.7504767,-60.3751797 -31.7504767,-60.3760144 -31.7509289,-60.3767292 -31.7513077,-60.3773037 -31.7516132,-60.3791301 -31.7526027,-60.3800814 -31.7531259,-60.3809362 -31.7535362,-60.3817033 -31.7538232,-60.3823594 -31.7540462,-60.3831037 -31.7542452,-60.383768 -31.7544097,-60.3844073 -31.7544919,-60.3852769 -31.7545847,-60.391759 -31.7550254,-60.3947969 -31.7552621,-60.3983017 -31.7555028,-60.4034161 -31.7557909,-60.4089999 -31.7561462,-60.4162907 -31.7566254,-60.4162907 -31.7566254,-60.4211499 -31.7569596,-60.4211499 -31.7569596,-60.4212419 -31.7569647,-60.4212419 -31.7569647,-60.4217818 -31.7569876,-60.422201 -31.7570016,-60.4225995 -31.7569933,-60.4229164 -31.7569808,-60.4231487 -31.7569695,-60.4234931 -31.7569398,-60.4237977 -31.7569098,-60.4240403 -31.7568805,-60.4244326 -31.7568285,-60.429089 -31.7561341,-60.4320429 -31.755709,-60.4320429 -31.755709,-60.4322796 -31.7556719,-60.4322796 -31.7556719,-60.433141 -31.7555326,-60.4335918 -31.7554554,-60.4338563 -31.7554001,-60.4342514 -31.7553105,-60.4398157 -31.7538303,-60.4405065 -31.7536485,-60.4408899 -31.7535552,-60.4413162 -31.7534647,-60.4418103 -31.7533804,-60.4425572 -31.7532894,-60.4435896 -31.753165,-60.4487216 -31.7525757,-60.4543628 -31.7519507,-60.4550763 -31.7518676,-60.455633 -31.7517995,-60.4563331 -31.7517481,-60.4565781 -31.751726,-60.4568612 -31.7517154,-60.4570931 -31.7517238,-60.4573522 -31.7517406,-60.4576725 -31.7517808,-60.45802 -31.7518464,-60.4583806 -31.751929,-60.4586472 -31.7520081,-60.4590053 -31.7521396,-60.4593369 -31.7522893,-60.4596348 -31.7524335,-60.4599121 -31.7525919,-60.4601533 -31.7527553,-60.4603896 -31.7529481,-60.4606191 -31.7531376,-60.4608146 -31.7533219,-60.4610099 -31.7535311,-60.4613042 -31.7539019,-60.4614993 -31.7542016,-60.4616335 -31.7544454,-60.4617322 -31.7546647,-60.4618385 -31.7549542,-60.4623042 -31.7563725,-60.4625074 -31.7569435,-60.4626164 -31.7572098,-60.4627576 -31.7574969,-60.4629091 -31.7577689,-60.4630744 -31.7580066,-60.4632542 -31.7582587,-60.4633955 -31.758441,-60.4636021 -31.7586751,-60.4637868 -31.7588719,-60.4642818 -31.7593688,-60.4642818 -31.7593688,-60.4657159 -31.7607494,-60.4657159 -31.7607494,-60.4659275 -31.7609647,-60.4659275 -31.7609647,-60.4676762 -31.7626358,-60.4676762 -31.7626358,-60.4704237 -31.765323,-60.4714476 -31.7662934,-60.4727492 -31.7675396,-60.4729745 -31.7677357,-60.4731879 -31.7679059,-60.4734519 -31.7680892,-60.4737067 -31.768242,-60.4741758 -31.7685163,-60.4744872 -31.768657,-60.4748118 -31.7687893,-60.475228 -31.7689283,-60.4755843 -31.7690173,-60.4758954 -31.7690903,-60.476293 -31.7691631,-60.476293 -31.7691631,-60.4766998 -31.7692137,-60.4822966 -31.7696248,-60.4822966 -31.7696248,-60.4829569 -31.7696704,-60.4882929 -31.7700365,-60.4900467 -31.7701689,-60.4904031 -31.7702105,-60.4907066 -31.7702578,-60.4909641 -31.770308,-60.4912993 -31.7703894,-60.4915193 -31.7704493,-60.4915778 -31.7704649,-60.4915778 -31.7704649,-60.4916427 -31.7704822,-60.4916427 -31.7704822,-60.4916909 -31.770495,-60.491994 -31.7706067,-60.4926391 -31.7708823,-60.4943447 -31.7716185,-60.4945109 -31.7716747,-60.4945109 -31.7716747,-60.4945838 -31.771718,-60.4952189 -31.772001,-60.4960156 -31.7723578,-60.4967335 -31.7726567,-60.49705 -31.7727752,-60.4973223 -31.7728665,-60.49775 -31.7729713,-60.4981407 -31.7730473,-60.4986835 -31.7731218,-60.4992668 -31.7731626,-60.5007513 -31.7732472,-60.5007513 -31.7732472,-60.5037251 -31.7734225,-60.5042092 -31.7734492,-60.5048177 -31.7734662,-60.5051449 -31.7734753,-60.5054104 -31.7734753,-60.5058064 -31.7734505,-60.5063707 -31.7733886,-60.5068671 -31.7733006,-60.507315 -31.7731792,-60.5077413 -31.7730512,-60.5080395 -31.7729346,-60.5084842 -31.7727228,-60.5088276 -31.772529,-60.5092397 -31.7722801,-60.5095732 -31.7720319,-60.5099031 -31.7717468,-60.5102817 -31.7713813,-60.5119563 -31.7692915,-60.5125696 -31.768537,-60.512967 -31.7680465,-60.5160883 -31.7641966,-60.5165764 -31.7635889,-60.5176685 -31.7622364,-60.5190501 -31.7605006,-60.5196965 -31.7596572,-60.5204974 -31.7585817,-60.5204974 -31.7585817,-60.5207303 -31.758246,-60.5211648 -31.7576076,-60.5212465 -31.7575002,-60.5219138 -31.7564919,-60.52226 -31.7559724,-60.5233517 -31.7543968,-60.5239439 -31.7535624,-60.5253634 -31.7515949,-60.5264399 -31.7500995,-60.5290275 -31.7465343,-60.5294917 -31.7458866,-60.5295443 -31.7458132,-60.531725 -31.742812,-60.531908 -31.7426207,-60.5322666 -31.7421254))"
};

std::string
getWKT(const polygon_t &polygon) {
    std::stringstream ss;
    ss << std::setprecision(12) << boost::geometry::wkt(polygon);
    // std::cout << ss.str() << std::endl;
    return ss.str();
}

std::string
getWKTFromDB(const std::string &table, const long id, std::shared_ptr<Pq> &db) {
    auto result = db->query("SELECT ST_AsText(geom, 4326), refs from relations where osm_id=" + std::to_string(id));
    for (auto r_it = result.begin(); r_it != result.end(); ++r_it) {
        // std::cout << (*r_it)[0].as<std::string>() << std::endl;
        return (*r_it)[0].as<std::string>();
    }
}

int
main(int argc, char *argv[])
{
    logger::LogFile &dbglogfile = logger::LogFile::getDefaultInstance();
    dbglogfile.setWriteDisk(true);
    dbglogfile.setLogFilename("raw-test.log");
    dbglogfile.setVerbosity(3);

    const std::string dbconn{getenv("UNDERPASS_TEST_DB_CONN")
                                    ? getenv("UNDERPASS_TEST_DB_CONN")
                                    : "user=underpass_test host=localhost password=underpass_test"};

    TestPlanet test_planet;
    test_planet.init_test_case(dbconn);
    auto db = std::make_shared<Pq>();

    if (db->connect(dbconn + " dbname=underpass_test")) {
        auto queryraw = std::make_shared<QueryRaw>(db);
        std::map<long, std::shared_ptr<osmobjects::OsmWay>> waycache;

        processFile("raw-case-1.osc", db);
        processFile("raw-case-2.osc", db);

        std::string waysIds = "101874,101875";
        queryraw->getWaysByIds(waysIds, waycache);

        // 4 created Nodes, 1 created Way (same changeset)
        if ( getWKT(waycache.at(101874)->polygon).compare(expectedGeometries[0]) == 0) {
            runtest.pass("4 created Nodes, 1 created Ways (same changeset)");
        } else {
            runtest.fail("4 created Nodes, 1 created Ways (same changeset)");
            return 1;
        }

        // 1 created Way, 4 existing Nodes (different changeset)
        if ( getWKT(waycache.at(101875)->polygon).compare(expectedGeometries[1]) == 0) {
            runtest.pass("1 created Way, 4 existing Nodes (different changesets)");
        } else {
            runtest.fail("1 created Way, 4 existing Nodes (different changesets)");
            return 1;
        }

        // 1 modified node, indirectly modify other existing ways
        processFile("raw-case-3.osc", db);
        waycache.erase(101875);
        queryraw->getWaysByIds(waysIds, waycache);
        if ( getWKT(waycache.at(101875)->polygon).compare(expectedGeometries[2]) == 0) {
            runtest.pass("1 modified Node, indirectly modify other existing Ways (different changesets)");
        } else {
            runtest.fail("1 modified Node, indirectly modify other existing Ways (different changesets) - - raw-case-3.osc");
            return 1;
        }

        // 1 created Relation referencing 1 created Way and 1 existing Way
        processFile("raw-case-4.osc", db);
        if ( getWKTFromDB("relations", 211766, db).compare(expectedGeometries[3]) == 0) {
            runtest.pass("1 created Relation referencing 1 created Way and 1 existing Way (different changesets)");
        } else {
            runtest.fail("1 created Relation referencing 1 created Way and 1 existing Way (different changesets) - - raw-case-4.osc");
            return 1;
        }

        // 1 modified Node, indirectly modify other existing Ways and 1 Relation
        processFile("raw-case-5.osc", db);
        if ( getWKTFromDB("relations", 211766, db).compare(expectedGeometries[4]) == 0) {
            runtest.pass("1 modified Node, indirectly modify other existing Ways and 1 Relation (different changesets)");
        } else {
            runtest.fail("1 modified Node, indirectly modify other existing Ways and 1 Relation (different changesets) - raw-case-5.osc");
            return 1;
        }

        // 4 created Nodes, 2 created Ways, 1 created Relation with type=multilinestring
        processFile("raw-case-6.osc", db);
        if ( getWKTFromDB("relations", 211776, db).compare(expectedGeometries[5]) == 0) {
            runtest.pass("4 created Nodes, 2 created Ways, 1 created Relation with type=multilinestring (same changeset)");
        } else {
            runtest.fail("4 created Nodes, 2 created Ways, 1 created Relation with type=multilinestring (same changeset) - raw-case-6.osc");
            return 1;
        }

        // Complex, 1 polygon relation made of multiple ways
        processFile("raw-case-7.osc", db);
        if ( getWKTFromDB("relations", 17331328, db).compare(expectedGeometries[6]) == 0) {
            runtest.pass("Complex, 1 polygon relation made of multiple ways (same changeset) - raw-case-7.osc");
        } else {
            runtest.fail("Complex, 1 polygon relation made of multiple ways (same changeset) - raw-case-7.osc");
            return 1;
        }

        
        processFile("raw-case-8.osc", db);
        if ( getWKTFromDB("relations", 16191459, db).compare(expectedGeometries[7]) == 0) {
            runtest.pass("Complex, 1 polygon relation made of multiple ways (same changeset) - raw-case-8.osc");
        } else {
            runtest.fail("Complex, 1 polygon relation made of multiple ways (same changeset) - raw-case-8.osc");
            return 1;
        }

        processFile("raw-case-9.osc", db);
        if ( getWKTFromDB("relations", 16193116, db).compare(expectedGeometries[8]) == 0) {
            runtest.pass("Complex, 2 polygon relation made of multiple ways (same changeset) - raw-case-9.osc");
        } else {
            runtest.fail("Complex, 2 polygon relation made of multiple ways (same changeset) - raw-case-9.osc");
            return 1;
        }

        processFile("raw-case-10.osc", db);
        if ( getWKTFromDB("relations", 16770204, db).compare(expectedGeometries[9]) == 0) {
            runtest.pass("MultiLinestring made of multiple ways (same changeset) - raw-case-10.osc");
        } else {
            runtest.fail("MultiLinestring made of multiple ways (same changeset) - raw-case-10.osc");
            return 1;
        }

        processFile("raw-case-11.osc", db);
        if ( getWKTFromDB("relations", 16768791, db).compare(expectedGeometries[10]) == 0) {
            runtest.pass("MultiLinestring made of multiple ways (same changeset) - raw-case-11.osc");
        } else {
            runtest.fail("MultiLinestring made of multiple ways (same changeset) - raw-case-11.osc");
            return 1;
        }

        processFile("raw-case-12.osc", db);
        if ( getWKTFromDB("relations", 16766330, db).compare(expectedGeometries[11]) == 0) {
            runtest.pass("MultiPolygon made of multiple ways (same changeset) - raw-case-12.osc");
        } else {
            runtest.fail("MultiPolygon made of multiple ways (same changeset) - raw-case-12.osc");
            return 1;
        }


        processFile("raw-case-13.osc", db);
        if ( getWKTFromDB("relations", 17060812, db).compare(expectedGeometries[12]) == 0) {
            runtest.pass("MultiPolygon made of multiple ways (same changeset) - raw-case-13.osc");
        } else {
            runtest.fail("MultiPolygon made of multiple ways (same changeset) - raw-case-13.osc");
            return 1;
        }

        processFile("raw-case-14.osc", db);
        if ( getWKTFromDB("relations", 357043, db).compare(expectedGeometries[13]) == 0) {
            runtest.pass("MultiPolygon made of multiple ways (same changeset) - raw-case-14.osc");
        } else {
            runtest.fail("MultiPolygon made of multiple ways (same changeset) - raw-case-14.osc");
            return 1;
        }

        processFile("raw-case-15.osc", db);
        if ( getWKTFromDB("relations", 13341377, db).compare(expectedGeometries[14]) == 0) {
            runtest.pass("MultiPolygon made of multiple ways (same changeset) - raw-case-15.osc");
        } else {
            runtest.fail("MultiPolygon made of multiple ways (same changeset) - raw-case-15.osc");
            return 1;
        }


    }



}