--
--  $Id$
--
--  This file is part of the OpenLink Software Virtuoso Open-Source (VOS)
--  project.
--
--  Copyright (C) 1998-2006 OpenLink Software
--
--  This project is free software; you can redistribute it and/or modify it
--  under the terms of the GNU General Public License as published by the
--  Free Software Foundation; only version 2 of the License, dated June 1991.
--
--  This program is distributed in the hope that it will be useful, but
--  WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
--  General Public License for more details.
--
--  You should have received a copy of the GNU General Public License along
--  with this program; if not, write to the Free Software Foundation, Inc.,
--  51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
--
--
ECHO BOTH "STARTED: SyncML tests\n";

SET ARGV[0] 0;
SET ARGV[1] 0;
-- SyncML test

DROP TABLE DB.DBA.sync_input;
CREATE TABLE DB.DBA.sync_input(id INTEGER NOT NULL,dt LONG VARBINARY,PRIMARY KEY(id));

FOREACH HEXADECIMAL BLOB INSERT INTO DB.DBA.sync_input(id,dt) VALUES(1,?);
0200006A1D2D2F2F53594E434D4C2F2F4454442053796E634D4C20312E302F2F
454E6D6C71C303312E300172C30A53796E634D4C2F312E300165C30331333901
5BC30131016E57C318687474703A2F2F3139322E3136382E312E313A36363636
2F01016757C313494D45493A353734373137323431343032323901014E5A0001
53C31173796E636D6C3A617574682D6261736963010100004FC30C5A4746324F
6D526864673D3D01015A00014CC305313030303001010100006B464BC3013101
4FC30332303001546E57C30B2E2F63616C656E6461722F01016757C3182E2F43
5C53797374656D5C446174615C43616C656E64617201015A0001454AC3103230
303331323034543039333030375A014FC3103230303331323034543039333135
375A01010101010000120101
END

FOREACH HEXADECIMAL BLOB INSERT INTO DB.DBA.sync_input(id,dt) VALUES(2,?);
0200006A1D2D2F2F53594E434D4C2F2F4454442053796E634D4C20312E302F2F
454E6D6C71C303312E300172C30A53796E634D4C2F312E300165C30331333901
5BC30132016E57C318687474703A2F2F3139322E3136382E312E313A36363636
2F01016757C313494D45493A353734373137323431343032323901015A00014C
C305313030303001010100006B694BC30131015CC30131014CC30130014AC307
53796E63486472016FC313494D45493A35373437313732343134303232390168
C318687474703A2F2F3139322E3136382E312E313A363636362F014FC3033230
300101694BC30132015CC30131014CC30132014AC305416C657274016FC3182E
2F435C53797374656D5C446174615C43616C656E6461720168C30B2E2F63616C
656E6461722F014FC30332303001544F0001454FC31D323030332D31322D3034
5431313A33313A35352E3030302B30323A3030010101010100006A4BC3013301
6E57C30B2E2F63616C656E6461722F01016757C3182E2F435C53797374656D5C
446174615C43616C656E64617201015A00014D49C30A37363234363938303536
0148C30835393536373935330101010000604BC30134015A000153C310746578
742F782D7663616C656E64617201010000546757C3013201014FC3817B424547
494E3A5643414C454E4441520D0A56455253494F4E3A312E300D0A424547494E
3A564556454E540D0A5549443A320D0A4445534352495054494F4E3A74657374
730D0A445453544152543A3230303331313237543039303030300D0A4454454E
443A3230303331313237543039303030300D0A582D45504F434147454E444145
4E545259545950453A4150504F494E544D454E540D0A434C4153533A5055424C
49430D0A44435245415445443A3230303331313238543030303030300D0A4C41
53542D4D4F4449464945443A3230303331323031543132333530300D0A454E44
3A564556454E540D0A454E443A5643414C454E4441520D0A010101604BC30135
015A000153C310746578742F782D7663616C656E64617201010000546757C301
3301014FC38200424547494E3A5643414C454E4441520D0A56455253494F4E3A
312E300D0A424547494E3A564556454E540D0A5549443A330D0A444553435249
5054494F4E3A7465737473206D6F72650D0A445453544152543A323030333131
3238543039303030300D0A4454454E443A323030333131323854313930303030
0D0A582D45504F434147454E4441454E545259545950453A4150504F494E544D
454E540D0A434C4153533A5055424C49430D0A44435245415445443A32303033
31313238543030303030300D0A4C4153542D4D4F4449464945443A3230303331
323031543132333530300D0A454E443A564556454E540D0A454E443A5643414C
454E4441520D0A010101604BC30136015A000153C310746578742F782D766361
6C656E64617201010000546757C3013501014FC38207424547494E3A5643414C
454E4441520D0A56455253494F4E3A312E300D0A424547494E3A564556454E54
0D0A5549443A350D0A4445534352495054494F4E3A746F64617920696E746567
726174696F6E0D0A445453544152543A3230303331323031543039303030300D
0A4454454E443A3230303331323031543039303030300D0A582D45504F434147
454E4441454E545259545950453A4150504F494E544D454E540D0A434C415353
3A5055424C49430D0A44435245415445443A3230303331323031543030303030
300D0A4C4153542D4D4F4449464945443A323030333132303154313235343030
0D0A454E443A564556454E540D0A454E443A5643414C454E4441520D0A010101
01120101
END

FOREACH HEXADECIMAL BLOB INSERT INTO DB.DBA.sync_input(id,dt) VALUES(3,?);
0200006A1D2D2F2F53594E434D4C2F2F4454442053796E634D4C20312E302F2F
454E6D6C71C303312E300172C30A53796E634D4C2F312E300165C30331333901
5BC30133016E57C318687474703A2F2F3139322E3136382E312E313A36363636
2F01016757C313494D45493A353734373137323431343032323901015A00014C
C305313030303001010100006B694BC30131015CC30132014CC30130014AC307
53796E63486472016FC313494D45493A35373437313732343134303232390168
C318687474703A2F2F3139322E3136382E312E313A363636362F014FC3033230
300101694BC30132015CC30132014CC30138014AC30453796E63016FC3182E2F
435C53797374656D5C446174615C43616C656E6461720168C30B2E2F63616C65
6E6461722F014FC3033230300101120101
END

vhost_remove (lpath=>'/');
vhost_define (lpath=>'/', ppath=>'/DAV/', is_dav=>1);

DAV_MAKE_DIR ('/DAV/calendar/', http_dav_uid (), null, '110110110N');
DB.DBA.SYNC_MAKE_DAV_DIR ('vcalendar_11', DAV_SEARCH_ID ('/DAV/calendar/','C'), 'calendar', '/DAV/calendar/', '1.0');
DAV_RES_UPLOAD ('/DAV/syncml.dtd', file_to_string ('syncml.dtd'), '', '110100100N', 'dav', 'dav', 'dav', 'dav');

delete from sync_session;
delete from sync_anchors;
delete from sync_maps;
delete from sync_devices;
delete from sync_rplog;

delete from WS.WS.SYS_DAV_RES where RES_FULL_PATH like '/DAV/calendar/%';
select count(*) from WS.WS.SYS_DAV_RES where RES_FULL_PATH like '/DAV/calendar/%';
ECHO BOTH $IF $EQU $LAST[1] 0 "PASSED" "***FAILED";
SET ARGV[$LIF] $+ $ARGV[$LIF] 1;
ECHO BOTH ": Repository is empty cnt=" $LAST[1] "\n";

create procedure DB.DBA.SYNC_GET_AUTH_TYPE (in devid any)
  {
    return 1;
  };

create procedure test_syncml ()
  {
    declare ret any;
    declare result any;
    result_names   (result);
    for select id, dt from sync_input order by id do
    {
      declare ses any;
      ses := string_output ();
      ret := http_get ('http://localhost:$U{HTTPPORT}/', null, 'POST',
      'Content-Type: application/vnd.syncml+wbxml', blob_to_string (dt));
      ret := WBXML2XML (ret);
      --http ('<!DOCTYPE SyncML SYSTEM "http://localhost:$U{HTTPPORT}/syncml.dtd">', ses);
      --http (serialize_to_UTF8_xml (ret), ses);
      --dbg_obj_print (string_output_string (ses));
--    xml_validate_dtd (ses, 0, '', 'UTF-8', 'x-any', 'Validation=RIGOROUS Fsa=ERROR FsaBadWs=IGNORE BuildStandalone=ENABLE SignalOnError=ENABLE');
      result (id);
    }
  }
;

test_syncml ();
ECHO BOTH $IF $EQU $STATE OK  "PASSED" "***FAILED";
SET ARGV[$LIF] $+ $ARGV[$LIF] 1;
ECHO BOTH ": SyncML session done  STATE=" $STATE " MESSAGE=" $MESSAGE "\n";

select count(*) from WS.WS.SYS_DAV_RES where RES_FULL_PATH like '/DAV/calendar/%';
ECHO BOTH $IF $EQU $LAST[1] 3 "PASSED" "***FAILED";
SET ARGV[$LIF] $+ $ARGV[$LIF] 1;
ECHO BOTH ": Repository is filled cnt=" $LAST[1] "\n";

select count(*) from sync_anchors;
ECHO BOTH $IF $EQU $LAST[1] 1 "PASSED" "***FAILED";
SET ARGV[$LIF] $+ $ARGV[$LIF] 1;
ECHO BOTH ": Anchors are set no=" $LAST[1] "\n";

select count(*) from sync_maps;
ECHO BOTH $IF $EQU $LAST[1] 3 "PASSED" "***FAILED";
SET ARGV[$LIF] $+ $ARGV[$LIF] 1;
ECHO BOTH ": Mapping is estabilished cnt=" $LAST[1] "\n";

select count(*) from sync_rplog;
ECHO BOTH $IF $EQU $LAST[1] 3 "PASSED" "***FAILED";
SET ARGV[$LIF] $+ $ARGV[$LIF] 1;
ECHO BOTH ": Items are logged cnt=" $LAST[1] "\n";

select count(*) from sync_devices;
ECHO BOTH $IF $EQU $LAST[1] 1 "PASSED" "***FAILED";
SET ARGV[$LIF] $+ $ARGV[$LIF] 1;
ECHO BOTH ": Device is registered cnt=" $LAST[1] "\n";

DB.DBA.VHOST_REMOVE (lpath=>'/');
DB.DBA.VHOST_DEFINE ('*ini*', '*ini*', '/', '/', 0, 0, NULL,  NULL, NULL, NULL, 'dba', NULL, NULL, 0);

ECHO BOTH "COMPLETED WITH " $ARGV[0] " FAILED, " $ARGV[1] " PASSED: SyncML tests\n";
