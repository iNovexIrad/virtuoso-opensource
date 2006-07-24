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

create method ti_http_debug_print (in _caption varchar) returns any for WV.WIKI.TOPICINFO
{
  http(sprintf('<table><caption>%V</caption>', cast (_caption as varchar)));
  http(sprintf('<tr><th>default cluster</th><td>%V</td></tr>', cast (self.ti_default_cluster as varchar)));
  http(sprintf('<tr><th>raw name</th><td>%V</td></tr>', cast (self.ti_raw_name as varchar)));
  http(sprintf('<tr><th>raw title</th><td>%V</td></tr>', cast (self.ti_raw_title as varchar)));
  http(sprintf('<tr><th>wiki name</th><td>%V</td></tr>', cast (self.ti_wiki_name as varchar)));
  http(sprintf('<tr><th>cluster name</th><td>%V</td></tr>', cast (self.ti_cluster_name as varchar)));
  http(sprintf('<tr><th>local name</th><td>%V</td></tr>', cast (self.ti_local_name as varchar)));
  http(sprintf('<tr><th>id</th><td>%V</td></tr>', cast (self.ti_id as varchar)));
  http(sprintf('<tr><th>cluster id</th><td>%V</td></tr>', cast (self.ti_cluster_id as varchar)));
  http(sprintf('<tr><th>res id</th><td>%V</td></tr>', cast (self.ti_res_id as varchar)));
  http(sprintf('<tr><th>col id</th><td>%V</td></tr>', cast (self.ti_col_id as varchar)));
  http(sprintf('<tr><th>author id</th><td>%V</td></tr>', cast (self.ti_author_id as varchar)));
  http('</table>');
}
;

create method ti_complete_env () returns any for WV.WIKI.TOPICINFO
{
  if (get_keyword ('BASECLUSTER', self.ti_env) is null)
    self.ti_env := vector_concat ( vector ('BASECLUSTER', self.ti_cluster_name), self.ti_env);
  if (get_keyword ('INCLUDINGCLUSTER', self.ti_env) is null)
    self.ti_env := vector_concat ( vector ('INCLUDINGCLUSTER', self.ti_cluster_name), self.ti_env);
  if (get_keyword ('BASETOPIC', self.ti_env) is null)
    self.ti_env := vector_concat ( vector ('BASETOPIC', self.ti_local_name), self.ti_env);
  if (get_keyword ('INCLUDINGTOPIC', self.ti_env) is null)
    self.ti_env := vector_concat ( vector ('INCLUDINGTOPIC', self.ti_local_name), self.ti_env);
  if (get_keyword ('ATTACHURL', self.ti_env) is null)
    self.ti_env := vector_concat ( vector ('ATTACHURL', concat (self.ti_base_adjust, self.ti_cluster_name, '/', self.ti_local_name)), self.ti_env);
}
;

create method ti_xslt_vector () returns any for WV.WIKI.TOPICINFO
{
  return self.ti_xslt_vector (NULL);
}
;

create method ti_xslt_vector (in params any) returns any for WV.WIKI.TOPICINFO
{
  declare _res any;
  _res := vector (
    'ti_default_cluster'	, cast (self.ti_default_cluster as varchar),
    'ti_raw_name'		, cast (self.ti_raw_name as varchar),
    'ti_raw_title'		, cast (self.ti_raw_title as varchar),
    'ti_wiki_name'		, cast (self.ti_wiki_name as varchar),
    'ti_cluster_name'		, cast (self.ti_cluster_name as varchar),
    'ti_local_name'		, cast (self.ti_local_name as varchar),
    'ti_id'			, cast (self.ti_id as varchar),
    'ti_cluster_id'		, cast (self.ti_cluster_id as varchar),
    'ti_res_id'			, cast (self.ti_res_id as varchar),
    'ti_col_id'			, cast (self.ti_col_id as varchar),
    'ti_abstract'		, cast (self.ti_abstract as varchar),
    'ti_text'			, WV.WIKI.DELETE_SYSINFO_FOR (cast (self.ti_text as varchar), NULL),
    'ti_author_id'		, cast (self.ti_author_id as varchar),
    'ti_author'			, cast (self.ti_author as varchar),
    'ti_curuser_wikiname'	, cast (self.ti_curuser_wikiname as varchar),
    'ti_curuser_username'	, cast (self.ti_curuser_username as varchar),
    'ti_attach_col_id'		, cast (self.ti_attach_col_id as varchar),
    'ti_attach_col_id_2'	, cast (self.ti_attach_col_id_2 as varchar),
    'ti_mod_time'		, cast (self.ti_mod_time as varchar),
    'ti_e_mail'			, cast (self.ti_e_mail as varchar) );
--    'ti_entity'			, serialize_to_UTF8_xml (self.ti_get_entity (null, 0)) );
  _res := vector_concat (_res, self.ti_env);
  if (params is not null)
    _res := vector_concat (_res, params);
     
  return _res;
}
;

create method ti_parse_raw_name () returns any for WV.WIKI.TOPICINFO
{
  declare _colon, _dot integer;
  _colon := strchr (self.ti_raw_name, ':');
  if (_colon is not null)
    {
      self.ti_wiki_name := subseq (self.ti_raw_name, 0, _colon);
      self.ti_cluster_name := '';
      self.ti_local_name := subseq (self.ti_raw_name, _colon + 1);
      return;
    }
  self.ti_wiki_name := null;
  _dot := strchr (self.ti_raw_name, '.');
  if (_dot is null)
    {
      self.ti_cluster_name := self.ti_default_cluster;
      self.ti_local_name := self.ti_raw_name;
    }
  else
    {
      self.ti_cluster_name := subseq (self.ti_raw_name, 0, _dot);
      self.ti_local_name := subseq (self.ti_raw_name, _dot + 1);
    }
  return null;
}
;

create method ti_fill_cluster_by_name () returns any for WV.WIKI.TOPICINFO
{
  for select top 1 ClusterId, ColId, ColHistoryId, ColAttachId, ColXmlId, AdminId
  from WV.WIKI.CLUSTERS where ClusterName = self.ti_cluster_name do
    {
      self.ti_cluster_id := ClusterId;
      self.ti_col_id := ColId;
      self.ti_col_history_id := ColHistoryId;
      self.ti_col_attach_id := ColAttachId;
      self.ti_col_xml_id := ColXmlId;
      self.ti_cluster_admin_id := AdminId;
    }
  return null;
}
;

create method ti_fill_cluster_by_id () returns any for WV.WIKI.TOPICINFO
{
  for select top 1 ClusterName, ColId, ColHistoryId, ColAttachId, ColXmlId, AdminId
  from WV.WIKI.CLUSTERS where ClusterId = self.ti_cluster_id do
    {
      self.ti_cluster_name := ClusterName;
      self.ti_col_id := ColId;
      self.ti_col_history_id := ColHistoryId;
      self.ti_col_attach_id := ColAttachId;
      self.ti_col_xml_id := ColXmlId;
      self.ti_cluster_admin_id := AdminId;
    }
  return null;
}
;

create method ti_find_id_by_local_name () returns any for WV.WIKI.TOPICINFO
{
  declare _colon, _dot integer;
  declare _id integer;
  self.ti_id := coalesce (
    (select TopicId from WV.WIKI.TOPIC where LocalName = self.ti_local_name and ClusterId = self.ti_cluster_id),
    (select TopicId from WV.WIKI.TOPIC where LocalName2 = self.ti_local_name and ClusterId = self.ti_cluster_id),
    0 );
}
;

create method ti_find_id_by_raw_title () returns any for WV.WIKI.TOPICINFO
{
  declare _colon, _dot integer;
  declare _id integer;
  declare _test WV.WIKI.TOPICINFO;
  _test := WV.WIKI.TOPICINFO();
  _test.ti_raw_name := self.ti_raw_title;
  _test.ti_cluster_name := self.ti_default_cluster;
  _test.ti_default_cluster := self.ti_default_cluster;
  _test.ti_parse_raw_name();
  _test.ti_fill_cluster_by_name();
  _test.ti_find_id_by_local_name();
  if (_test.ti_id <> 0)
    self.ti_id := _test.ti_id;
  return null;
}
;

create method ti_find_metadata_by_id () returns any for WV.WIKI.TOPICINFO
{
  for select top 1 ClusterId, ResId, ResXmlId, TopicTypeId, LocalName, LocalName2, TitleText, Abstract, MailBox, ParentId
  from WV.WIKI.TOPIC where TopicId = self.ti_id do {
    self.ti_cluster_id := ClusterId;
    self.ti_res_id := ResId;
    self.ti_type_id := TopicTypeId;
    self.ti_local_name := LocalName;
    self.ti_local_name_2 := LocalName2;
    self.ti_title_text := TitleText;
    self.ti_abstract := Abstract;
    self.ti_e_mail := MailBox;
    self.ti_parent_id := ParentId;
  }
  --dbg_obj_print ('before:',self.ti_rev_id);
  self.ti_fill_cluster_by_id();
  --dbg_obj_print ('after:',self.ti_rev_id);
  
  self.ti_author_id := NULL;

  declare _mod_time datetime;
  declare _author varchar; 
  declare _author_id int;

  declare _dav_auth, _dav_pwd varchar;
  WV.WIKI.GETDAVAUTH (_dav_auth, _dav_pwd);

  if (self.ti_res_id <> 0)
    {
      declare content, type varchar;
      declare path varchar;
      declare author_id int;
      if (self.ti_rev_id <> 0)
	{
	  path := DB.DBA.DAV_SEARCH_PATH (self.ti_col_id, 'C') || 'VVC/' || self.ti_local_name || '.txt/' ||
   	     cast (self.ti_rev_id as varchar);
	  self.ti_author_id := (select U_ID from WS.WS.SYS_DAV_RES_VERSION, DB.DBA.SYS_USERS where RV_WHO = U_NAME and RV_RES_ID = self.ti_res_id and RV_ID = self.ti_rev_id);	  
	  --dbg_obj_print ('xxx');	
	} 
      else
	{
          path := DB.DBA.DAV_SEARCH_PATH (self.ti_res_id, 'R');
	  self.ti_author_id := DAV_HIDE_ERROR (DAV_PROP_GET (path, ':virtowneruid', _dav_auth, _dav_pwd));
	}
      if (self.ti_author_id is null)
	{
	  declare main_path varchar;
          main_path := DB.DBA.DAV_SEARCH_PATH (self.ti_res_id, 'R');
	  self.ti_author_id := DAV_HIDE_ERROR (DAV_PROP_GET (main_path, ':virtowneruid', _dav_auth, _dav_pwd));
	}
      if (0 < DB.DBA.DAV_RES_CONTENT (path, content, type, _dav_auth, _dav_pwd))
        { 
          self.ti_text := cast (content as varchar);
        }
      else
        WV.WIKI.APPSIGNAL (11001, 'Can not get topic content (revision: ' || case when self.ti_rev_id = 0 then 'last' else cast (self.ti_rev_id as varchar) end || ')',  vector () );
	  --dbg_obj_print ('xxx 2');	
      self.ti_mod_time := DAV_HIDE_ERROR (DAV_PROP_GET (path, ':getlastmodified', _dav_auth, _dav_pwd));
	  --dbg_obj_print ('xxx y', self.ti_author_id);	
      self.ti_author := coalesce ( 
	(select AuthorName from WV.WIKI.TOPIC where ResId = self.ti_res_id), 
	WV.WIKI.USER_WIKI_NAME_2(self.ti_author_id), 
	'Unknown');
      declare dav_path varchar;
      dav_path := DAV_HIDE_ERROR (DAV_SEARCH_PATH (self.ti_res_id, 'R'));
      if (dav_path is not null)
	{ 
	  dav_path := subseq (dav_path, 0, length(dav_path) - 4) || '/';
	  self.ti_attach_col_id := coalesce (DAV_HIDE_ERROR (DAV_SEARCH_ID (dav_path, 'C')), 0);
	}

    }
  return null;
}
;

create method ti_find_metadata_by_res_id () returns any for WV.WIKI.TOPICINFO
{
  declare _topic_id int;
  _topic_id := (select TopicId from WV.WIKI.TOPIC where ResId = self.ti_res_id);
  if (_topic_id is null)
    self.ti_id := 0;
  else
    {
      self.ti_id := _topic_id;
      self.ti_find_metadata_by_id();
    }
}
;

create method ti_full_name () returns varchar for WV.WIKI.TOPICINFO
{
  return self.ti_cluster_name || '.' || self.ti_local_name;
}
;

create method ti_run_lexer (in _env any) returns varchar for WV.WIKI.TOPICINFO
{
  declare exit handler for sqlstate '*' {
    --dbg_obj_print (__SQL_STATE, __SQL_MESSAGE);
    return '';
  }
  ;
  declare _text varchar;
  if (isstring (self.ti_text))
    _text := self.ti_text;
  else
    _text := cast (self.ti_text as varchar);
  _text := WV.WIKI.DELETE_SYSINFO_FOR (_text, NULL);
  declare _res varchar;
  _res := "WikiV lexer" (_text || '\r\n', 
  	coalesce (self.ti_cluster_name, 'Main'),
	coalesce (self.ti_local_name, 'WelcomeVisitors'),
	self.ti_curuser_wikiname, _env);
  return _res;
}
;

create method ti_get_entity (in _env any, in _ext int) returns any for WV.WIKI.TOPICINFO
{

  --dbg_obj_print ('get_entity');
  declare _html varchar;
  declare _ent any;
  if (_env is null)
    {
  --dbg_obj_print ('get_entity 1');
      self.ti_complete_env();
  --dbg_obj_print ('get_entity 2');
      _env := self.ti_env;
  --dbg_obj_print ('get_entity 3');
    }  
  _html := self.ti_run_lexer (_env);
  --dbg_obj_print ('get_entity 4');

  _ent := xtree_doc (_html, 2, '', 'UTF-8');


  --dbg_obj_print ('get_entity 5', _ent);
  if (_ext = 1) 
    {	
        --dbg_obj_print ('get_entity 6');
	XMLAppendChildren (_ent, self.ti_report_attachments());
        --dbg_obj_print ('get_entity 7');
	XMLAppendChildren (_ent, self.ti_wiki_path());
        --dbg_obj_print ('get_entity 8');
	XMLAppendChildren (_ent, self.ti_report_mails());
        --dbg_obj_print ('get_entity 9');
	XMLAppendChildren (_ent, self.ti_revisions(0, 7));
        --dbg_obj_print ('get_entity 10');
	XMLAppendChildren (_ent, self.ti_get_tags());
        --dbg_obj_print ('get_entity 11');
    }
  --dbg_obj_print (_ent);
  if (_ent is null)
    _ent := XMLELEMENT('xmp', _html);
  --dbg_obj_print ('get_entity 6', _ent);
  return _ent;
}
;

create method ti_compile_page () returns any for WV.WIKI.TOPICINFO
{
  declare exit handler for sqlstate '*' {
    --dbg_obj_print (__SQL_STATE, __SQL_MESSAGE);
    resignal;
  }
  ;
  --dbg_obj_print ('ti_compile_page 1', self);
  declare _ent any;
  declare _abstract nvarchar;
  declare _diskdumpdir any; -- Name of directory to dump pages; for debugging purposes.
  _diskdumpdir := registry_get ('WikiDiskDump');
  if (isstring (_diskdumpdir))
    {
      declare err any;
      file_mkdir (_diskdumpdir, err);
      file_mkdir (_diskdumpdir || '/' || self.ti_cluster_name, err);
      string_to_file (
        concat (_diskdumpdir, '/', self.ti_cluster_name, '/', self.ti_local_name, '.txt'),
        coalesce (self.ti_text, ''), -2);
    }
  --dbg_obj_print ('ti_compile_page 2');

  if (self.ti_author_id is null or 0 = self.ti_author_id)
    self.ti_author_id := coalesce ((select U_ID from DB.DBA.SYS_USERS where U_NAME=connection_get ('vspx_user')), 0);

  self.ti_local_name_2 := WV.WIKI.SINGULARPLURAL (self.ti_local_name);
  --dbg_obj_print ('ti_compile_page 3');

  self.ti_curuser_wikiname := 'WikiGuest';
  --dbg_obj_print ('ti_compile_page 4');
  self.ti_curuser_username := 'WikiGuest';
  --dbg_obj_print ('ti_compile_page 5');
  self.ti_base_adjust := '';
  --dbg_obj_print ('ti_compile_page 6');
  _ent := self.ti_get_entity (null,1);
  --dbg_obj_print ('ti_compile_page 7', _ent);
  _abstract := xpath_eval ('string(//abstract)', _ent);
  --dbg_obj_print ('ti_compile_page 8');
  if (_abstract = N'')
    _abstract := null;
  --dbg_obj_print ('ti_compile_page 9');
  
  --dbg_obj_print ('ti_compile_page 4444');
  insert replacing WV.WIKI.TOPIC (TopicId, ClusterId, ResId, LocalName, LocalName2, Abstract, MailBox)
  values (self.ti_id, self.ti_cluster_id, self.ti_res_id, self.ti_local_name, self.ti_local_name_2, _abstract, self.ti_e_mail);
  if (connection_get ('oWiki import') is not null)
    {
      declare _links, _qlinks, _flinks any;
      _links := xpath_eval ('//a[@style="wikiword"][@href]', _ent, 0);
      _qlinks := xpath_eval ('//a[@style="qwikiword"][@href]', _ent, 0);
      _flinks := xpath_eval ('//a[@style="forcedwikiword"][@href]', _ent, 0);

      -- make link with DestId NULL for later processing
      WV.WIKI.update_links (self, _links, 0);
      WV.WIKI.update_links (self, _qlinks, 1);
      WV.WIKI.update_links (self, _flinks, 2);
      return;
    }
-- Processing links
  delete from WV.WIKI.LINK where OrigId = self.ti_id and MadeByDest = 0;
  delete from WV.WIKI.LINK where DestId = self.ti_id and MadeByDest = 1;  
  delete from WV.WIKI.SEMANTIC_OBJ where SO_OBJECT_ID = self.ti_id;
  
  {
    declare _links any;
    declare _ctr, _count integer;
    _links := xpath_eval ('//a[@style="wikiword"][@href] | //a[@style="qwikiword"][@href] | //a[@style="forcedwikiword"][@href]', _ent, 0);
    _count := length (_links);
    _ctr := 0;
    declare _categories varchar;
    _categories := '';
    while (_ctr < _count)
      {
	declare _a any;
	declare _href, _linktext varchar;
	declare _tgt WV.WIKI.TOPICINFO;
	_a := aref (_links, _ctr);
	_href := xpath_eval ('@href', _a);
	_linktext := cast (_a as varchar);
	if (length (_linktext) > 200)
	  _linktext := concat (subseq (_linktext, 0, 100 + coalesce (strchr (subseq (_linktext, 100), ' '), 0)), ' ...');
	if (_href like 'Category%' or
	    _href like '%.Category%')
	  {
	    _categories := case when _categories = '' then cast (_href as varchar) else _categories || ' ' || cast (_href as varchar) end;
	  }
	_tgt := WV.WIKI.TOPICINFO ();
	_tgt.ti_raw_title := _href;
	_tgt.ti_default_cluster := self.ti_cluster_name;
	_tgt.ti_find_id_by_raw_title ();
	if (_tgt.ti_id <> 0)
          {
	    _tgt.ti_find_metadata_by_id ();
          }
	else
	  {
	    _tgt.ti_raw_name := _href;
	    _tgt.ti_cluster_name := self.ti_default_cluster;
	    _tgt.ti_default_cluster := self.ti_default_cluster;
	    _tgt.ti_parse_raw_name();
	  }
	if (not exists 
	  (select top 1 1 from WV.WIKI.LINK
	    where OrigId = self.ti_id and TypeId = 0 and MadeByDest = 0 and
	      DestClusterName = _tgt.ti_cluster_name and
	      DestLocalName = _tgt.ti_local_name ) )
	  {
	    insert into WV.WIKI.LINK (LinkId, TypeId, OrigId, DestId, DestClusterName, DestLocalName, MadeByDest, LinkText)
	    values (WV.WIKI.NEWPLAINLINKID(), 0, self.ti_id, _tgt.ti_id, _tgt.ti_cluster_name, _tgt.ti_local_name, 0, _linktext);
	  }
	declare _pred varchar;
	_pred := xpath_eval ('@predicate', _a);
	if (_pred is not null)
	  {
	    
	    WV.WIKI.ADD_FACT (self, cast (_pred as varchar), _tgt.ti_local_name, ':TOPIC');
	  }
        _ctr := _ctr + 1;
      }
    declare _facts, _link any;
    _facts := xpath_eval ('//span[@style="semanticvalue"]', _ent, 0);
    _count := length (_facts);
    _ctr := 0;    
    while (_ctr < _count)
      {
        _link := _facts[_ctr];
        declare _pred varchar;
	declare _value varchar;
	 --dbg_obj_princ ('span=', _link);
	_pred := xpath_eval ('@predicate', _link);
	_value := xpath_eval ('@value', _link);
	if (_pred is not null and _value is not null)
	  {
	    WV.WIKI.ADD_FACT (self, cast (_pred as varchar), cast (_value as varchar), ':VALUE');
	  }
	_ctr := _ctr + 1;
      }
    update WV.WIKI.LINK set DestId = self.ti_id
    where DestId = 0 and DestClusterName = self.ti_cluster_name and
      (DestLocalName = self.ti_local_name or DestLocalName = self.ti_local_name_2 );
    if (_categories <> '' and
	(WV.WIKI.CLUSTERPARAM (self.ti_cluster_id, 'delicious_enabled', 2) = 1))
      WV.WIKI.DELICIOUSPUBLISH (self.ti_id, split_and_decode (_categories, 0, '\0\0 '));
  }
}
;

create procedure WV.WIKI.UPDATE_LINKS (
	in _topic WV.WIKI.TOPICINFO,
	in _links any,
	in _type int) -- 0 - WikiWord, 1 - Qualified WW, 2 - forcedlink
{
  foreach (any _a in _links) do
    {
      WV.WIKI.UPDATE_LINK_1 (_topic, cast (xpath_eval ('@href', _a) as varchar), _type);
    }
}
;

  
create procedure WV.WIKI.UPDATE_LINK_1 (
	in _topic WV.WIKI.TOPICINFO,
	in _href varchar,
	in _type int) -- 0 - WikiWord, 1 - Qualified WW, 2 - forcedlink
{
  declare _cluster varchar;
  declare _local_name varchar;
   --dbg_obj_princ ('link:{{ ', _topic.ti_local_name, ' ', _href, ' ', _type);
  

  if (_type = 0)
    {
      _cluster := _topic.ti_cluster_name;
      _local_name := _href;
    }
  else if (_type = 1)
    {
      declare _aux any;
      _aux := split_and_decode (_href, 0, '\0\0.');      
      _cluster := _aux[0];
      _local_name := _aux[1];
    }
  else if (_type = 2)
    {
      _cluster := _topic.ti_cluster_name;
      _local_name := trim (_href);
    }
  else
    return;
  if (not exists (select * from WV.WIKI.LINK where
	OrigId = _topic.ti_id
	and DestClusterName = _cluster
	and DestLocalName = _local_name))
    {
      --dbg_obj_princ ('}}done');
      insert into WV.WIKI.LINK (LinkId, TypeId, OrigId, DestId, DestClusterName, DestLocalName, MadeByDest, LinkText)
	values (WV.WIKI.NEWPLAINLINKID(), 0, _topic.ti_id, NULL, _cluster, _local_name, 0, NULL);
    }
  return;
}
;
  	
 
create function WV.WIKI.POSTPROCESS_LINKS (in _cluster_id int)
{
  for select LinkId as _link_id, c.ClusterId as DestClusterId, 
  	DestLocalName, t.LocalName as FromLocalName,
	DestClusterName
   from WV.WIKI.LINK inner join WV.WIKI.TOPIC t
    on (t.TopicId = OrigId) inner join WV.WIKI.CLUSTERS c
    on (c.ClusterName = DestClusterName)
    where t.ClusterId = _cluster_id
     and DestId is null
  do {
    for select top 1 TopicId from WV.WIKI.TOPIC 
      where LocalName = DestLocalName
      and ClusterId = DestClusterId
    do {
      update WV.WIKI.LINK set DestId = TopicId
       where LinkId = _link_id;
       --dbg_obj_princ (_link_id, ' new link from ', FromLocalName, ' to ', DestClusterName, '.', DestLocalName);
    }
  }
  delete from WV.WIKI.LINK 
    where DestId is NULL or DestId = 0 
    and exists (select * from WV.WIKI.TOPIC where TopicId = OrigId and ClusterId = _cluster_id);
}
;


-- _res_is_vect means result in vector
-- _total - number of version in report, 0 means not such contraint
create method ti_revisions(in _res_is_vect int, in _total int) returns any for WV.WIKI.TOPICINFO
{
  declare exit handler for sqlstate '*' {
    --dbg_obj_print (__SQL_STATE, __SQL_MESSAGE);
    resignal;
  }
  ;
  declare _res, _ent any;
  declare path, revs varchar;
  if (_res_is_vect)
    vectorbld_init (_res);
  else
    _res := XMLELEMENT ('Versions');
  path := DB.DBA.DAV_SEARCH_PATH (self.ti_col_id, 'C') || 'VVC/' || self.ti_local_name || '.txt/';
  revs := DB.DBA.DAV_DIR_LIST (path, 0, 'dav', (select pwd_magic_calc (U_NAME, U_PWD, 1) from WS.WS.SYS_DAV_USER where U_ID = http_dav_uid()));
  declare _max, _min int;
  _max := 0;
  _min := -1;
  if (isarray (revs))
    {
      if (not _res_is_vect)
        _ent := xpath_eval ('/Versions', _res);
      declare idx int;
      for (idx := length (revs) - 1 ; idx >= 0; idx:=idx - 1)
        {	
	  declare _file any;
	  _file := aref (revs, idx);
	  if ( (aref (_file, 1) = 'R') or (aref (_file, 1) = 'r') )
	    {
	      if ( ( aref (_file, 10) <> 'history.xml' ) and
	      	   ( aref (_file, 10) <> 'last' ) and
		   ( aref (_file, 10) not like '%.diff') )
	        {
		  declare _num int;
		  _num := atoi (aref (_file, 10));
		  if (_min < 0)
		    _min := _num;
		  if (_num > _max)
		    _max := _num;
		  if (_num < _min)
		    _min := _num;
	 	}
	    }
	}
      --dbg_obj_print (_max, _min);
      if (_total and (_max - _min > WV.WIKI.MAX_REVS_IN_REPORT()))
	{
	  _min := _max - WV.WIKI.MAX_REVS_IN_REPORT();
	  XMLAppendChildren (_ent, XMLELEMENT ('RevCont'));
        }
      if (_min > 0)
  	{
	  for (idx := _min; idx <= _max; idx := idx + 1)
	    {
	      declare _i int;
	      _i := idx;
	      if (_res_is_vect)
	        vectorbld_acc (_res, cast (_i as varchar));
	      else
	  	XMLAppendChildren (_ent, XMLELEMENT ('Rev', 
		 	XMLATTRIBUTES (idx as Number)));
	    }
	}
    }  
  if (_res_is_vect)
    vectorbld_final (_res);
  return _res;
}
;

create function WV.WIKI.MAX_REVS_IN_REPORT ()
{ 
  return 2;
}
;
--  xtree_doc ('<Versions><Rev Number="1"/><Diff/><Rev Number="2"/></Versions>'), self.ti_report_mails());

create method ti_report_attachments () returns any for WV.WIKI.TOPICINFO
{
  declare _dav_path varchar;
  _dav_path := DB.DBA.DAV_SEARCH_PATH (self.ti_col_id, 'C') || self.ti_local_name || '/';

  declare _dir_list, _res, _ent, _attachment any;
  _dir_list := DAV_DIR_LIST (_dav_path, 0, 'dav', (select pwd_magic_calc (U_NAME, U_PWD, 1) from WS.WS.SYS_DAV_USER where U_ID = http_dav_uid()));

  _res := XMLELEMENT ('ATTACHMENTS');
  _ent := xpath_eval ('/ATTACHMENTS', _res);

  if (_dir_list is null)
	return _res;
  if (not isarray (_dir_list))
	return _res;
  foreach (any _file in _dir_list) do {
	if ( (aref (_file, 1) = 'R') or (aref (_file, 1) = 'r') ) {
		_attachment := XMLELEMENT ('Attach',
				XMLATTRIBUTES (
				 self.ti_cluster_name as Cluster,
				 self.ti_local_name as Topic,
				 aref (_file, 10) as Name,
				 aref (_file, 0) as Path,
				 WV.WIKI.DATEFORMAT (aref (_file, 8)) as "ModTime",
				 WV.WIKI.PRINTLENGTH (aref (_file, 2)) as "Size",
				 WV.WIKI.DATEFORMAT (aref (_file, 3)) as "Date",
				 aref (_file, 9) as Type,
				 aref (_file, 5) as Permissions,
				 (select UserName from WV.WIKI.USERS where UserId = aref (_file, 7)) as Owner),
				XMLELEMENT ('Comment', coalesce ((select Description from WV.WIKI.ATTACHMENTINFONEW where ResPath = aref (_file, 0)), '')));
		XMLAppendChildren (_ent, _attachment);
   	}
  }
  return _res;
}
;

create method ti_report_mails () returns any for WV.WIKI.TOPICINFO
{
  connection_set ('WIKIV Cluster', self.ti_cluster_name);
  return WV.WIKI.MAILBOXLIST (self.ti_id);
}
;

create procedure WV.WIKI.DOTREEPARENT (in _topic_id int, in _ent any, in depth int)
{
  if (depth > WV.WIKI.MAXPARENTDEPTH())
    return _ent;
  for select top 1 ParentId, LocalName, ClusterName 
	       from WV.WIKI.TOPIC t, WV.WIKI.CLUSTERS c
	       where TopicId = _topic_id 
	       and c.ClusterId = t.ClusterId
	       
  do {	       
    XMLAppendChildren (_ent, 
       XMLELEMENT ('Parent',
	   XMLATTRIBUTES (ClusterName as CLUSTERNAME,
			  LocalName as LOCALNAME,
			  depth as DEPTH)));
    if (ParentId > 0) {
      WV.WIKI.DOTREEPARENT (ParentId, _ent, depth + 1);
    } else {
      XMLAppendChildren (_ent, 
			 XMLELEMENT ('Parent',
				     XMLATTRIBUTES (ClusterName as CLUSTERNAME,
						    '' as LOCALNAME,
						    (depth + 1) as DEPTH)));
      XMLAppendChildren (_ent, 
			 XMLELEMENT ('Parent',
				     XMLATTRIBUTES ('Main' as CLUSTERNAME,
						    WV.WIKI.DASHBOARD() as LOCALNAME,
						    (depth + 2) as DEPTH)));
    }
  }
}
;
					  
create method ti_wiki_path () returns any for WV.WIKI.TOPICINFO
{
  if (self.ti_local_name = WV.WIKI.DASHBOARD() and self.ti_cluster_name = 'Main')
    return xtree_doc ('<WikiPath><Parent CLUSTERNAME="Main" LOCALNAME="Dashboard"/></WikiPath>');
  declare _doc any;
  _doc := xtree_doc ('<WikiPath></WikiPath>');
  WV.WIKI.DOTREEPARENT (self.ti_id, xpath_eval ('/WikiPath', _doc), 0);
  return _doc;
  
}
;


create constructor method TOPICINFO () for WV.WIKI.TOPICINFO
{
  -- These assignments are there due to bugs in assigning default values
  -- to instance. Briefly speaking, '...default XXX' in member declaration may
  -- be ignored by server. Weird bug.
  self.ti_id := 0;
  self.ti_res_id := 0;
  self.ti_type_id := 0;
  self.ti_cluster_id := 0;
  self.ti_col_id := 0;
  self.ti_col_history_id := 0;
  self.ti_col_attach_id := 0;
  self.ti_col_xml_id := 0;
  self.ti_cluster_admin_id := 0;
  self.ti_author_id := 0;
  self.ti_curuser_wikiname := 'WikiEngineAdmin';
  self.ti_curuser_username := 'Wiki';
  self.ti_base_adjust := '';
  self.ti_attach_col_id := 0;
  self.ti_attach_col_id_2 := 0;
  self.ti_env := vector();
}
;

create method ti_res_name () for WV.WIKI.TOPICINFO
{
  return self.ti_local_name || '.txt';
}
;

create method ti_update_text (in _text varchar, in _auth varchar) for WV.WIKI.TOPICINFO
{
  declare _owner varchar;
  select U_NAME into _owner from WS.WS.SYS_DAV_RES, DB.DBA.SYS_USERS 
	where RES_ID = self.ti_res_id
	and U_ID = RES_OWNER;
  
  WV.WIKI.UPLOADPAGE (
     self.ti_col_id,
     self.ti_res_name(),
     _text,
     _owner,
     self.ti_cluster_id,
     _auth);
  return NULL;
}
;

create method ti_full_path () for WV.WIKI.TOPICINFO
{
  declare _full_path varchar;
  _full_path := (select RES_FULL_PATH from WS.WS.SYS_DAV_RES where RES_ID = self.ti_res_id);
  if (_full_path is null)
    return DB.DBA.DAV_SEARCH_PATH (self.ti_col_id, 'C') || self.ti_local_name || '.txt';
  return _full_path;
}
;

create method ti_get_tags () for WV.WIKI.TOPICINFO
{
  --dbg_obj_print ('get_tags');
  
  declare exit handler for sqlstate '*' {
    --dbg_obj_print (__SQL_STATE, __SQL_MESSAGE);
    resignal;
  }
  ;
  declare _tags any;
  declare _nobody_uid, _curr_uid int;
  --dbg_obj_print ('get_tags 1');
  _nobody_uid := (select U_ID from DB.DBA.SYS_USERS where U_NAME = 'nobody');
  --dbg_obj_print ('get_tags 2');
  _curr_uid := (select U_ID from DB.DBA.SYS_USERS where U_NAME = self.ti_curuser_username);
  --dbg_obj_print ('get_tags 3');
  
  
  _tags := DAV_HIDE_ERROR (DAV_TAG_LIST (self.ti_res_id, 'R', vector (_curr_uid, _nobody_uid)));
  --dbg_obj_print (_tags);
  if (_tags is not null)
    {
      declare _public_tags, _pub_ent any;
      declare _private_tags, _priv_ent any;
      _public_tags := XMLELEMENT ('tagset', XMLATTRIBUTES ('public' as "type"));
      _private_tags := XMLELEMENT ('tagset', XMLATTRIBUTES ('private' as "type"));
      _pub_ent := xpath_eval ('/tagset', _public_tags);
      _priv_ent := xpath_eval ('/tagset', _private_tags);
      foreach (any taginfo in _tags) do
        {
	  foreach (varchar tag in split_and_decode (taginfo[1], 0, '\0\0,')) do
	    {
	      if (taginfo[0] = _nobody_uid) -- public tags
	        XMLAppendChildren (_pub_ent, XMLELEMENT ('tag', XMLATTRIBUTES (tag as "name")));
	      else
	        XMLAppendChildren (_priv_ent, XMLELEMENT ('tag', XMLATTRIBUTES (tag as "name")));
	    }
	}
      return XMLELEMENT ('tags', _public_tags, _private_tags);
    }
  return NULL;
}
;


-- Triggers

create trigger "Wiki_ClusterInsert" after insert on WS.WS.SYS_DAV_PROP order 100 referencing new as N
{
  declare exit handler for sqlstate '*' {
 	resignal;
  }; 
  declare _cname varchar;
  declare _src_col integer;
  declare _owner integer;
  declare _group integer;
  if (N.PROP_NAME = 'WikiCluster')
    {
      _cname := N.PROP_VALUE;
      _src_col := N.PROP_PARENT_ID;
      select COL_OWNER, COL_GROUP into _owner, _group from WS.WS.SYS_DAV_COL where COL_ID = _src_col;
      if (_group is null)
	_group := WV.WIKI.WIKIADMINGID();
      WV.WIKI.CREATECLUSTER (_cname, _src_col, _owner, _group);
    }
}
;

create trigger "Wiki_ClusterDelete" before delete on WS.WS.SYS_DAV_PROP order 100 referencing old as O
{
  declare exit handler for sqlstate '*' {
 	resignal;
  }; 
  if (O.PROP_NAME = 'WikiCluster')
    {
      for select ClusterId as _cid, ColId as _col_id from WV.WIKI.CLUSTERS where ClusterName = O.PROP_VALUE or ColId = O.PROP_PARENT_ID do {
	  DeleteCluster (_cid);
	}
    }  
}
;

create trigger "Wiki_ClusterDeleteContent" before delete on WV.WIKI.CLUSTERS referencing old as O
{
  declare exit handler for sqlstate '*' {
 	resignal;
  }; 
  DB.DBA.DAV_DELETE (WS.WS.COL_PATH(O.ColId), 1, 'dav', (select pwd_magic_calc (U_NAME, U_PWD, 1) from WS.WS.SYS_DAV_USER where U_ID = http_dav_uid()));
  delete from WA_INSTANCE where WAI_TYPE_NAME = 'oWiki' and (WAI_INST as wa_wikiv).cluster_id = O.ClusterId;
  delete from WV.WIKI.CLUSTERSETTINGS where ClusterId = O.ClusterId;
  delete from WA_MEMBER where WAM_APP_TYPE = 'oWiki' and WAM_INST = O.ClusterName;
  DB.DBA.USER_ROLE_DROP(O.ClusterName || 'Readers');
  DB.DBA.USER_ROLE_DROP(O.ClusterName || 'Writers');
  DB.DBA.VHOST_REMOVE(lpath=>'/wiki/' || O.ClusterName);
}
;


create trigger "Wiki_ClusterUpdate" before update on WS.WS.SYS_DAV_PROP order 100 referencing old as O, new as N
{
  if (O.PROP_NAME = 'WikiCluster' or N.PROP_NAME = 'WikiCluster')
    {
      WV.WIKI.APPSIGNAL (11001, 'Cluster "&ClusterName;" can not be changed by updating DAV property WikiCluster',
	 vector ('ClusterName', O.PROP_VALUE) );
    }
}
;

create trigger "Wiki_TopicTextInsertPerms" after insert on WS.WS.SYS_DAV_RES order 999 referencing new as N
{
  declare _cluster_name varchar;
  _cluster_name :=  (select ClusterName from WV.WIKI.TOPIC natural join WV.WIKI.CLUSTERS
	where ResId = N.RES_ID);
  if (_cluster_name is null)
   return;

  SET TRIGGERS OFF;
  WV.WIKI.UPDATEGRANTS_FOR_RES_OR_COL ( _cluster_name, N.RES_ID, 'R');
  SET TRIGGERS ON;
}
;

create trigger "Wiki_TopicTextUpdatePerms" after insert on WS.WS.SYS_DAV_RES order 999 referencing new as N
{
  declare _cluster_name varchar;
  _cluster_name :=  (select ClusterName from WV.WIKI.TOPIC natural join WV.WIKI.CLUSTERS
	where ResId = N.RES_ID);
  if (_cluster_name is null)
   return;

  SET TRIGGERS OFF;
  WV.WIKI.UPDATEGRANTS_FOR_RES_OR_COL ( _cluster_name, N.RES_ID, 'R');
  SET TRIGGERS ON;
}
;


create trigger "Wiki_TopicTextInsertMeta" after insert on WS.WS.SYS_DAV_RES order 1 referencing new as N
{
  declare _cluster_name varchar;
  whenever not found goto skip;
  select ClusterName into _cluster_name from WV.WIKI.CLUSTERS where ColId = N.RES_COL;
  connection_set ('oWiki Topic', N.RES_NAME);
  connection_set ('oWiki Cluster', _cluster_name);
  skip: ;
}
;


create trigger "Wiki_TopicTextInsert" after insert on WS.WS.SYS_DAV_RES order 100 referencing new as N
{
  declare exit handler for sqlstate '*' {
    --dbg_obj_princ (__SQL_STATE, __SQL_MESSAGE);
    resignal;
  };
  declare _newtopic WV.WIKI.TOPICINFO;
  declare _cluster_name varchar;
  whenever not found goto skip;
  select ClusterName into _cluster_name from WV.WIKI.CLUSTERS where ColId = N.RES_COL;
  _newtopic := WV.WIKI.TOPICINFO ();
  _newtopic.ti_cluster_name := _cluster_name;
  _newtopic.ti_fill_cluster_by_name ();
  _newtopic.ti_id := WV.WIKI.NEWPLAINTOPICID ();
  _newtopic.ti_res_id := N.RES_ID;
  _newtopic.ti_default_cluster := _cluster_name;
  _newtopic.ti_local_name := WV.WIKI.FILENAMETOWIKINAME (cast (N.RES_NAME as varchar));
  _newtopic.ti_text := cast (N.RES_CONTENT as varchar);
  _newtopic.ti_e_mail := WV.WIKI.MAILBOXFORTOPICNEW (_newtopic.ti_id, _cluster_name, _newtopic.ti_local_name);
  _newtopic.ti_compile_page ();
  connection_set ('oWiki Topic', N.RES_NAME);
  connection_set ('oWiki Cluster', _cluster_name);
  declare _perms varchar;
  _perms := N.RES_PERMS;
  --dbg_obj_princ ( WS.WS.ACL_PARSE (dav_prop_get ('/DAV/home/dav/wiki/Main/BlogFAQ.txt', ':virtacl', 'dav','dav')));
  declare exit handler for sqlstate '*' {
    --dbg_obj_princ (__SQL_STATE, ' ', __SQL_MESSAGE);
    rollback work;
    return;
  };
  --dbg_obj_princ (1, WS.WS.ACL_PARSE (dav_prop_get ('/DAV/home/dav/wiki/Main/BlogFAQ.txt', ':virtacl', 'dav','dav')));
  -- notify wa dashboard about the stuff
  if (exists (select * from DB.DBA.WA_MEMBER where
  	WAM_INST = _newtopic.ti_cluster_name
	and WAM_APP_TYPE = 'oWiki'
	and WAM_IS_PUBLIC = 1))
  if (__proc_exists ('DB.DBA.WA_NEW_WIKI_IN'))
     {
       declare _uname, _uid varchar;
       select U_FULL_NAME, U_NAME into _uname, _uid from DB.DBA.SYS_USERS where U_ID = N.RES_OWNER;
       _newtopic.ti_fill_url();
       DB.DBA.WA_NEW_WIKI_IN (WV.WIKI.NORMALIZEWIKIWORDLINK (_newtopic.ti_cluster_name, _newtopic.ti_local_name), _newtopic.ti_url || '?', _newtopic.ti_id);
       insert into WV.WIKI.DASHBORD (WD_TITLE, WD_UNAME, WD_UID, WD_URL)
	  values (subseq (_newtopic.ti_text, 0, 200), _uname, _uid, _newtopic.ti_url || '?');
     }
  --dbg_obj_princ (2,  WS.WS.ACL_PARSE (dav_prop_get ('/DAV/home/dav/wiki/Main/BlogFAQ.txt', ':virtacl', 'dav','dav')));
  skip: ;
}
;

create trigger "Wiki_TopicTextDelete" before delete on WS.WS.SYS_DAV_RES order 100 referencing old as O
{
  --dbg_obj_princ ('Wiki_TopicTextDelete: ', O.RES_ID, ' >  ', O.RES_FULL_PATH);
  declare exit handler for sqlstate '*' {
	--dbg_obj_print (__SQL_STATE, __SQL_MESSAGE);
  	resignal;
  };
  declare _id integer;
  whenever not found goto skip;
  select TopicId into _id from WV.WIKI.TOPIC where ResId = O.RES_ID;
  delete from WV.WIKI.SEMANTIC_OBJ where SO_OBJECT_ID = _id;
  WV.WIKI.DELETETOPIC (_id);
  skip: ;
}
;

create trigger "Wiki_TopicTextUpdate" after update on WS.WS.SYS_DAV_RES order 100 referencing old as O, new as N
{
  --dbg_obj_print ('Wiki_TopicTextUpdate', N.RES_FULL_PATH, connection_get ('oWiki trigger'));
  declare exit handler for sqlstate '*' {
	--dbg_obj_princ (__SQL_STATE, __SQL_MESSAGE);
 	resignal;
  }; 
  declare _id integer;
  _id := coalesce ((select TopicId from WV.WIKI.TOPIC where ResId = O.RES_ID), 0);
  if (O.RES_ID <> N.RES_ID or O.RES_COL <> N.RES_COL)
    {
      if (_id <> 0)
        WV.WIKI.DELETETOPIC (_id);
    }
  declare _newtopic WV.WIKI.TOPICINFO;
  declare _cluster_name, _local_name varchar;
  --dbg_obj_princ (1);
  whenever not found goto skip_insert;
  select ClusterName into _cluster_name from WV.WIKI.CLUSTERS where ColId = N.RES_COL;
  --dbg_obj_princ (2);
  _newtopic := WV.WIKI.TOPICINFO ();
  _newtopic.ti_cluster_name := _cluster_name;
  _newtopic.ti_fill_cluster_by_name ();
  _local_name := WV.WIKI.FILENAMETOWIKINAME (cast (N.RES_NAME as varchar));
  if (_id = 0)
    {
      _id := WV.WIKI.NEWPLAINTOPICID ();
      _newtopic.ti_e_mail := WV.WIKI.MAILBOXFORTOPICNEW (_id, _cluster_name, _local_name);
    }
  else
    _newtopic.ti_e_mail := (select MailBox from WV.WIKI.TOPIC where TopicId = _id);
  _newtopic.ti_id := _id;
  _newtopic.ti_res_id := N.RES_ID;
  _newtopic.ti_default_cluster := _cluster_name;
  _newtopic.ti_local_name := _local_name;
  _newtopic.ti_text := cast (N.RES_CONTENT as varchar);
--  dbg_obj_princ (3, _newtopic);
  _newtopic.ti_compile_page ();
  connection_set ('oWiki Topic', _local_name);
  connection_set ('oWiki Cluster', _cluster_name);
  skip_insert: ;
  --dbg_obj_princ (4);
}
;

wiki_exec_no_error ('drop trigger WS.WS.Wiki_AttachemntDelete')
;

create trigger "Wiki_AttachmentDelete" before delete on WS.WS.SYS_DAV_RES order 100 referencing old as O
{
	delete from WV.WIKI.ATTACHMENTINFONEW where ResPath = O.RES_FULL_PATH;
}
;

-- Renreding routines

create function WV.WIKI.NORMALIZEWIKIWORDLINK (
  inout _default_cluster varchar, inout _href varchar) returns varchar
{ -- Converts dirty WikiLink to a proper normalized qualified WikiLink.
  declare _topic WV.WIKI.TOPICINFO;
  _topic := WV.WIKI.TOPICINFO ();
  _topic.ti_raw_name := _href;
  _topic.ti_default_cluster := _default_cluster;
  _topic.ti_parse_raw_name ();
  return concat (_topic.ti_cluster_name, '.', _topic.ti_local_name);
}
;

grant execute on WV.WIKI.NORMALIZEWIKIWORDLINK to public
;

create function WV.WIKI.READONLYWIKIWORDLINK (
  in _default_cluster varchar, in _href varchar) returns varchar
{ -- Converts dirty WikiLink into link in form Cluster/LocalName
  declare _topic WV.WIKI.TOPICINFO;
  _topic := WV.WIKI.TOPICINFO ();
  _topic.ti_raw_name := _href;
  _topic.ti_default_cluster := _default_cluster;
  _topic.ti_parse_raw_name ();
  if (_topic.ti_cluster_name = '')
    return '';
  return _topic.ti_cluster_name || '/' || _topic.ti_local_name;
--  return '?WikiCluster=' || _topic.ti_cluster_name || '&WikiWord=' || _topic.ti_local_name;
}
;

create function WV.WIKI.READONLYWIKIWORDHREF (
  inout _default_cluster varchar, 
  inout _href varchar,
  in _sid varchar,
  in _realm varchar,
  in _base_adjust varchar,
  in _params any
) returns varchar
{ -- Converts dirty WikiLink into path of form Cluster/LocalName
  --dbg_obj_princ ('WV.WIKI.READONLYWIKIWORDHREF: ', _params);
  declare _topic WV.WIKI.TOPICINFO;
  _topic := WV.WIKI.TOPICINFO ();
  _topic.ti_raw_name := _href;
  _topic.ti_default_cluster := _default_cluster;
  _topic.ti_parse_raw_name ();
  declare url_params varchar;
  if (isstring(_params))  
    url_params :=  WV.WIKI.URL_PARAMS (WV.WIKI.COLLECT_PAIRS (_params, WV.WIKI.COLLECT_PAIRS (WV.WIKI.PAIR ('sid', _sid), WV.WIKI.PAIR('realm', _realm))));
  if (url_params <> '')
    return sprintf ('%s%U/%U?%s', _base_adjust, _topic.ti_cluster_name, _topic.ti_local_name, url_params);
  return sprintf ('%s%U/%U', _base_adjust, _topic.ti_cluster_name, _topic.ti_local_name);
}
;

grant execute on WV.WIKI.READONLYWIKIWORDLINK to public
;
grant execute on WV.WIKI.READONLYWIKIWORDHREF to public
;

xpf_extension ('http://www.openlinksw.com/Virtuoso/WikiV/:ReadOnlyWikiWordLink', 'WV.WIKI.READONLYWIKIWORDLINK')
;
xpf_extension ('http://www.openlinksw.com/Virtuoso/WikiV/:ReadOnlyWikiWordHREF', 'WV.WIKI.READONLYWIKIWORDHREF')
;


create function WV.WIKI.QUERYWIKIWORDLINK (
  inout _default_cluster varchar, inout _href varchar) returns varchar
{ -- Parses dirty WikiLink and returns the TopicId of the most suitable page.
  declare _topic WV.WIKI.TOPICINFO;
  _topic := WV.WIKI.TOPICINFO ();
  _topic.ti_raw_title := _href;
  _topic.ti_default_cluster := _default_cluster;
  _topic.ti_find_id_by_raw_title ();
  return _topic.ti_id;
}
;

grant execute on WV.WIKI.QUERYWIKIWORDLINK to public
;

xpf_extension ('http://www.openlinksw.com/Virtuoso/WikiV/:QueryWikiWordLink', 'WV.WIKI.QUERYWIKIWORDLINK')
;

create function WV.WIKI.EXPANDMACRO (
  inout _name varchar, inout _data varchar, inout _context any, inout _env any) returns varchar
{ -- Calls the implementation of macro and returns the composed XML fragment.
  --dbg_obj_princ ('WV.WIKI.EXPANDMACRO ', _name, _data, _context, _env);
  declare _funname varchar;
  declare _res any;
  declare exit handler for sqlstate '*' {
    if (__SQL_STATE = 'WVRLD')
      resignal;
    return XMLELEMENT (cast (_funname as varchar), xtree_doc (sprintf ('<div class="wiki-error"><h2>SQL STATE:</h2><h3><![CDATA[%s]]></h3><h2>SQL MESSAGE:</h2><h3><![CDATA[%s]]></h3></div>', __SQL_STATE, __SQL_MESSAGE)));
  }
  ;
  declare exit handler for not found {
    return XMLELEMENT (cast (_funname as varchar), xtree_doc (sprintf ('<div class="wiki-error"><h2>SQL STATE:</h2><h3><![CDATA[%s]]></h3></div>', 'nof found')));
  }
  ;   

  _funname := fix_identifier_case ('WV.Wiki.' || 'MACRO_' || replace (_name, ':', '_'));
  if (exists (select 1 from DB.DBA.SYS_PROCEDURES where P_NAME= _funname))
    _res := call (_funname) (_data, _context, _env);
  else _res := sprintf ('((The macro extension "%s" is not available on this server))', _name);
  if (not isentity (_res))
    _res := XMLELEMENT (cast (_funname as varchar), _res);
  return _res;
}
;

grant execute on WV.WIKI.EXPANDMACRO to public
;

xpf_extension ('http://www.openlinksw.com/Virtuoso/WikiV/:ExpandMacro', 'WV.WIKI.EXPANDMACRO')
;

create function WV.WIKI.GETENV (in _name varchar, inout _env any) returns varchar
{ -- Returns a string that is a value of WikiV environment variable from registry.
  declare _res any;
  _res := get_keyword (_name, _env);
  if (_res is not null)
    return _res;
  _res := registry_get (concat ('WikiV %', _name, '%'));
  if (_res is not null)
    return _res;
  return  sprintf ('?"%s"?', _name);
}
;

grant execute on WV.WIKI.GETENV to public
;

xpf_extension ('http://www.openlinksw.com/Virtuoso/WikiV/:GetEnv', 'WV.WIKI.GETENV')
;

-- Id allocators. They return new values of primary keys for their tables.

create function WV.WIKI.NEWCLUSTERID () returns integer
{
  declare _res integer;
  while (1)
    {
      _res := 10000000 + rand (89999999); -- exactly 8-digit integers
      if (not exists (select top 1 1 from WV.WIKI.CLUSTERS where ClusterId = _res))
	return _res;
    }
}
;

create function WV.WIKI.NEWPLAINTOPICID () returns integer
{
  declare _res integer;
  while (1)
    {
      _res := 100000000 + rand (899999999); -- exactly 9-digit integers
      if (not exists (select top 1 1 from WV.WIKI.TOPIC where TopicId = _res))
	  return _res;
    }
}
;

create function WV.WIKI.NEWPLAINLINKID () returns integer
{
  declare _res integer;
  while (1)
    {
      _res := 100000000 + rand (899999999); -- exactly 9-digit integers
      if (not exists (select top 1 1 from WV.WIKI.LINK where LinkId = _res))
	return _res;
    }
}
;

-- Administrative functions for DAV

create function WV.WIKI.WIKIUID () returns integer
{ -- Returns UID of 'Wiki' user
  return coalesce ((select U_ID from DB.DBA.SYS_USERS where U_NAME='Wiki' and U_IS_ROLE=0 and U_ACCOUNT_DISABLED=0 and U_DAV_ENABLE=1 and U_SQL_ENABLE=1), NULL);
}
;

create function WV.WIKI.WIKIADMINGID () returns integer
{ -- Returns UID of 'WikiAdmin' group
  return coalesce ((select U_ID from DB.DBA.SYS_USERS where U_NAME='WikiAdmin' and U_IS_ROLE=1 and U_DAV_ENABLE=1), NULL);
}
;

create function WV.WIKI.WIKIUSERGID () returns integer
{ -- Returns UID of 'WikiUser' group
  return coalesce ((select U_ID from DB.DBA.SYS_USERS where U_NAME='WikiUser' and U_IS_ROLE=1 and U_DAV_ENABLE=1), NULL);
}
;

create function WV.WIKI.CREATEDAVCOLLECTION (in _col_parent integer, in _col_name varchar, in _owner integer, in _group integer) returns integer
{ -- Creates _col_name collection in _col_parent.
-- Owner user and group can be NULL to indicate default,
-- default is 'Wiki' or (as the last resort) 'dav'.
  declare _res integer;
  if (_owner is null)
    _owner := coalesce (WV.WIKI.WIKIUID(), http_dav_uid());
  if (_group is null)
    _group := coalesce (WV.WIKI.WIKIADMINGID(), http_dav_uid() + 1);

 declare _res int;
  _res := DB.DBA.DAV_SEARCH_ID (DB.DBA.DAV_SEARCH_PATH (_col_parent, 'C') || _col_name || '/', 'C');
  if (_res > 0)
	return _res;
  _res := DB.DBA.DAV_COL_CREATE (DB.DBA.DAV_SEARCH_PATH (_col_parent, 'C') || _col_name || '/',
	'110100100R', 
	(select U_NAME from DB.DBA.SYS_USERS where U_ID = _owner),
	(select U_NAME from DB.DBA.SYS_USERS where U_ID = _group),
	'dav',
	(select pwd_magic_calc (U_NAME, U_PWD, 1) from WS.WS.SYS_DAV_USER where U_ID = http_dav_uid()));
  return _res;
}
;

-- Administrative functions for Wiki

create procedure WV.WIKI.CREATEGROUP (in _sysname varchar, in _name varchar, in _seccmt varchar, in signal_err int:=1)
{
  declare _gid integer;
  declare _oldname varchar;
  if (exists (select 1 from WV.WIKI.GROUPS where GroupName = _name))
    {
      if (signal_err = 1)
	WV.WIKI.APPSIGNAL (11001, 'Wiki group "&GroupName;" already exists.',
			       vector ('GroupName', _name) );
      else
	return;
    }
  _gid := coalesce ((select U_ID from DB.DBA.SYS_USERS where U_NAME = _sysname and U_IS_ROLE = 1), NULL);
  if (_gid is null) 
    {
      if (signal_err = 1)
	WV.WIKI.APPSIGNAL (11001, 'System group "&GName;" does not exist; can not create Wiki group "&GroupName;"',
				 vector ('GName', _sysname, 'GroupName', _name) );
      else
	return;
    }
  _oldname := coalesce ((select GroupName from WV.WIKI.GROUPS where GroupId = _gid), NULL);
  if (_oldname is not null)
    {
      if (signal_err= 1)
	WV.WIKI.APPSIGNAL (11001, 'Wiki group "&GroupName;" is not created because account "&GName;" is already known as "&OldGroupName;".',
				 vector ('GName', _sysname, 'OldGroupName', _oldname, 'GroupName', _name) );
      else
	return;
    }
  if (exists (select 1 from DB.DBA.SYS_USERS where U_NAME = _name and U_ID <> _gid) or
    exists (select 1 from WV.WIKI.USERS where UserName = _name) )
    WV.WIKI.APPSIGNAL (11001, 'Can not create Wiki group "&GroupName;" because a very similar name is already in use',
      vector ('GroupName', _name) );
  insert into WV.WIKI.GROUPS (GroupId, GroupName, SecurityCmt)
  values (_gid, _name, _seccmt);
}
;

create procedure WV.WIKI.CREATEUSER (in _sysname varchar, in _name varchar, in _group varchar, in _seccmt varchar, in signal_err int:=1)
{
  declare exit handler for sqlstate '42WV9' {
	if (signal_err)
		resignal;
	return;
  };
  _name := WV.WIKI.USER_WIKI_NAME (_name);
  declare _gid, _uid integer;
  declare _oldname varchar;
  _uid := coalesce ((select U_ID from DB.DBA.SYS_USERS where U_NAME = _sysname and U_IS_ROLE = 0 and U_ACCOUNT_DISABLED=0 and U_DAV_ENABLE=1), NULL);
  if (_uid is null)
    WV.WIKI.APPSIGNAL (11001, 'System user accout "&UName;" does not exist or can not be used for Wiki purposes; can not regsiter Wiki user "&UserName;"',
      vector ('UName', _sysname, 'UserName', _name) );
  if (not exists (select 1 from DB.DBA.SYS_USER_GROUP where UG_UID = _uid and UG_GID = WV.WIKI.WIKIUSERGID())
    and not exists (select 1 from DB.DBA.SYS_USERS where U_ID = _uid and U_GROUP = WV.WIKI.WIKIUSERGID()) )
    WV.WIKI.APPSIGNAL (11001, 'System user accout "&UName;" is not a member of WikiUser group and can not be used for Wiki purposes',
      vector ('UName', _sysname) );
  if (exists (select 1 from WV.WIKI.USERS where UserId = _uid))
    return;
again:
  if (exists (select 1 from WV.WIKI.USERS where UserName = _name))
    {
      _name := sprintf ('%s%d', _name, _uid);
      goto again;
    }
--	WV.WIKI.APPSIGNAL (11001, 'Wiki user name "&UserName;" is already registered.',
--				 vector ('UserName', _name) );
  _gid := coalesce ((select GroupId from WV.WIKI.GROUPS where GroupName = _group), NULL);
  if (_gid is null)
    WV.WIKI.APPSIGNAL (11001, 'Group "&GroupName;" does not exist; can not register Wiki user "&UserName;"',
      vector ('GroupName', _group, 'UserName', _name) );
  _oldname := coalesce ((select UserName from WV.WIKI.USERS where UserId = _uid), NULL);
  if (_oldname is not null)
    {
      delete from WV.WIKI.USERS where UserId = _uid;
    }
  if (exists (select 1 from DB.DBA.SYS_USERS where U_NAME = _name and U_ID <> _uid) or
    exists (select 1 from WV.WIKI.GROUPS where GroupName = _name and GroupName <> _group) )
    WV.WIKI.APPSIGNAL (11001, 'Can not register Wiki user name "&GroupName;" because a very similar name is already in use',
      vector ('UserName', _name) );
    
  insert into WV.WIKI.USERS (UserId, UserName, MainGroupId, SecurityCmt)
  values (_uid, _name, _gid, _seccmt);
}
;

create procedure WV.WIKI.CREATEROLES (in _cname varchar)
{
  declare st, msg any;
  EXEC ('DB.DBA.USER_ROLE_CREATE (''' || _cname || 'Readers'') ', st, msg);
  EXEC ('DB.DBA.USER_ROLE_CREATE (''' || _cname || 'Writers'') ', st, msg);
}
;

create procedure WV.WIKI.UPDATEACL (in _article varchar, in _gid int, in _bitmask int, in _auth_name varchar, in _auth_pwd varchar)
{
  --dbg_obj_princ ('UPDATEACL: ', _article, ' ', _gid, ' ', '_bitmask', ' ');
  declare _acl any;
  declare a_id int;
  a_id := DAV_SEARCH_ID (_article, 'R');
  if (a_id < 0) 
    return;
  _acl := DB.DBA.DAV_PROP_GET_INT(a_id, 'R', ':virtacl', 0);
  if (not isinteger (_acl))
    {
      declare _res int;
      declare _new_acl, _old_acl any;
      _acl := cast (_acl as varbinary);
      _old_acl := _acl;
      _new_acl := WS.WS.ACL_ADD_ENTRY(_old_acl, _gid, _bitmask, 1);
      --dbg_obj_print (ws.ws.ACL_PARSE(_acl), ws.ws.acl_parse(_new_acl));
      if (1 or _acl <> _new_acl)
        {
	  _acl := _new_acl;
	  --dbg_obj_princ (_article, _gid, _auth_name, _auth_pwd, _acl);
      	  _res := DB.DBA.DAV_PROP_SET_INT(_article, ':virtacl',  _acl, null, null, 0, 0, 0, http_dav_uid ());
      	  if (_res < 0)
        	signal ('WIKI00', sprintf ('Can not update ACL: %d %d',_res,coalesce ((select top 1 RES_OWNER from WS.WS.SYS_DAV_RES where RES_ID = (select RES_ID from WS.WS.SYS_DAV_RES where RES_FULL_PATH = _article)), 0))); 
	}
    }
  else
    signal ('WIKI01', ':virtacl property retrieval failed: ' || DAV_PERROR (_acl));
}
;

create procedure WV.WIKI.UPDATEGRANTS (in _cname varchar, in signalerror int:=0)
{
  declare _readers, _writers int;
  _readers := ( select U_ID from DB.DBA.SYS_USERS where U_NAME = _cname || 'Readers'
  			and U_IS_ROLE = 1 );
  _writers := ( select U_ID from DB.DBA.SYS_USERS where U_NAME = _cname || 'Writers'
  			and U_IS_ROLE = 1 );
  if ( (_readers is null) or (_writers is null) )
    {
      if (signalerror)
    signal ('WK002', 'No readers or writers group for ' || _cname);
      else
	 return;
    }
  for select DAV_HIDE_ERROR (DB.DBA.DAV_SEARCH_PATH (ResId, 'R')) as _path
    from WV.WIKI.TOPIC natural inner join WV.WIKI.CLUSTERS
    where clustername = _cname
  do {
    if (_path is not null) 
      {
    declare _owner, _pwd varchar;
    select U_NAME, pwd_magic_calc (U_NAME, U_PASSWORD, 1) into _owner, _pwd
        from DB.DBA.SYS_USERS inner join WS.WS.SYS_DAV_COL 
	  on (U_ID = COL_OWNER)	 
        where COL_NAME = _cname;
    WV.WIKI.UPDATEACL (_path, _writers, 6, _owner, _pwd);
    WV.WIKI.UPDATEACL (_path, _readers, 4, _owner, _pwd);
  }
  }
}
;

create procedure WV.WIKI.UPDATEGRANTS_FOR_RES_OR_COL (in _cname varchar, in _res_id int, in _type varchar(1):='R')
{
  declare _readers, _writers int;
  declare _path varchar;
  declare _owner, _pwd varchar;
  
  _readers := ( select U_ID from DB.DBA.SYS_USERS where U_NAME = _cname || 'Readers'
  			and U_IS_ROLE = 1 );
  _writers := ( select U_ID from DB.DBA.SYS_USERS where U_NAME = _cname || 'Writers'
  			and U_IS_ROLE = 1 );
  if ( (_readers is null) or (_writers is null) )
    signal ('XXXXX', 'No readers or writers group for ' || _cname);
  _path := DB.DBA.DAV_SEARCH_PATH (_res_id, _type);
  --dbg_obj_princ (':::' , _path);
  if (not isinteger (_path))
    {
      select U_NAME, pwd_magic_calc (U_NAME, U_PASSWORD, 1) into _owner, _pwd
        from DB.DBA.SYS_USERS inner join WS.WS.SYS_DAV_COL 
	  on (U_ID = COL_OWNER)	 
        where COL_NAME = _cname; 
--      declare _cluster_id int;
--      _cluster_id := (select ClusterId from WV.WIKI.CLUSTERS where ClusterName = _cname);
--     update WS.WS.SYS_DAV_COL set COL_PERMS = WV.WIKI.GETDEFAULTPERMS (_cluster_id)
--      	where COL_ID = _res_id;
      WV.WIKI.UPDATEACL (_path, _writers, 6, _owner, _pwd);
      WV.WIKI.UPDATEACL (_path, _readers, 4, _owner, _pwd);
    }
  else
    signal ('XXXX', 'path is unknown');
}
;
  

-- create all parent collections
create procedure WV.WIKI.ENSURE_DIR_REC (in _paths any, in _last_index int)
{
  --dbg_obj_princ ('WV.WIKI.ENSURE_DIR_REC ', _paths, _last_index);
  if (_last_index <= 2) -- /DAV
    return 1;
  declare _col_id int;
  declare _full_path varchar;
  _full_path := WV.WIKI.STRJOIN ('/', subseq (_paths, 0, _last_index)) || '/';
  _col_id := DAV_SEARCH_ID (_full_path, 'C');
  --dbg_obj_princ ('col_id: ', _col_id, _full_path);
  if (DAV_HIDE_ERROR(_col_id) is null)
    {
      if (WV.WIKI.ENSURE_DIR_REC (_paths, _last_index - 1) < 0)
        return -1;
      return  DB.DBA.DAV_MAKE_DIR (_full_path, http_dav_uid(), http_dav_uid() + 1, '110100100R');
    }
  return _col_id;
}
;
      
  

create procedure WV.WIKI.DAV_HOME_CREATE(in user_name varchar) returns varchar
{
  declare user_id varchar;
  declare user_home varchar;

  whenever not found goto error;
  select U_HOME into user_home  from DB.DBA.SYS_USERS where U_NAME = user_name;
  user_home := coalesce ( user_home, '/DAV/home/' || user_name || '/');
  
  declare _res_id int;
  _res_id := DAV_SEARCH_ID (user_home, 'C');
  if (DAV_HIDE_ERROR (_res_id) is not null)
    goto create_wiki_home;
  -- create home
  declare _last int;
  if (length(user_home) > 0)
    _last := user_home[length(user_home)-1];
  else
    _last := user_home[0];
  if (_last <> '/'[0])
    user_home := user_home || '/';
  
  declare _paths any;
  _paths := split_and_decode (user_home, 0, '\0\0/');  
  if (WV.WIKI.ENSURE_DIR_REC (_paths, length (_paths) - 1) < 0)
    return -18;
  _res_id := DB.DBA.DAV_MAKE_DIR (user_home, user_id, null, '110100100R');
  USER_SET_OPTION(user_name, 'HOME', user_home);

create_wiki_home:
  --create wiki home
  user_home := user_home || 'wiki/';
  _res_id := DAV_SEARCH_ID (user_home, 'C');
  if (DAV_HIDE_ERROR (_res_id) is not null)
    return _res_id;
  _res_id := DB.DBA.DAV_MAKE_DIR (user_home, user_id, null, '110100100R');

  return _res_id;
error:
  return -18;
}
;




create procedure WV.WIKI.CREATECLUSTER (in _cname varchar, in _src_col integer, in _owner integer, in _group integer, in signal_err int:=1)
{
  --dbg_obj_print ('1');
  declare exit handler for sqlstate '42WV9' {
  --dbg_obj_print ('1err');
	if (signal_err = 1)
	   resignal;
	return;
  };
  declare _uname, _gname, _home varchar;
  declare _wikiuname, _wikigname varchar;
  declare _parent, _main, _histcol, _xmlcol, _attachcol integer;
-- Preparing user name
  _uname := coalesce ((select U_NAME from DB.DBA.SYS_USERS where U_ID = _owner and U_IS_ROLE = 0), NULL);
  if (_uname is null)
    WV.WIKI.APPSIGNAL (11001, 'User ID "&UId;" is invalid; can not create cluster "&ClusterName;"',
      vector ('UId', _owner, 'ClusterName', _cname) );
  --dbg_obj_print ('2');

  if (exists (select 1 from DB.DBA.SYS_USERS where U_ID = _owner and U_ACCOUNT_DISABLED <> 0))
    WV.WIKI.APPSIGNAL (11001, 'Account "&UName;" is disabled; can not create cluster "&ClusterName;"',
      vector ('UName', _uname, 'ClusterName', _cname) );
  --dbg_obj_print ('4');

  if (exists (select 1 from DB.DBA.SYS_USERS where U_ID = _owner and U_DAV_ENABLE = 0))
    WV.WIKI.APPSIGNAL (11001, 'Account "&UName;" has no right to use DAV; can not create cluster "&ClusterName;"',
      vector ('UName', _uname, 'ClusterName', _cname) );	
  _wikiuname := coalesce ((select UserName from WV.WIKI.USERS where UserId = _owner), NULL);
  if (_wikiuname is null)
    WV.WIKI.APPSIGNAL (11001, 'User "&UserName;" is not a registered Wiki user; can not create cluster "&ClusterName;"',
      vector ('UserName', _uname, 'ClusterName', _cname) );
-- Preparing group name
  _gname := coalesce ((select U_NAME from DB.DBA.SYS_USERS where U_ID = _group and U_IS_ROLE = 1), NULL);
  if (_gname is null)
    WV.WIKI.APPSIGNAL (11001, 'Group ID "&GId;" is invalid; can not create cluster "&ClusterName;"',
      vector ('GId', _group, 'ClusterName', _cname) );
  --dbg_obj_print ('5');

  _wikigname := coalesce ((select GroupName from WV.WIKI.GROUPS where GroupId = _group), NULL);
  if (_wikigname is null)
    WV.WIKI.APPSIGNAL (11001, 'Group "&UserName;" is not a valid Wiki group; can not create cluster "&ClusterName;"',
      vector ('GName', _gname, 'ClusterName', _cname) );
-- Preparing parent for internal files
  _home := (select U_HOME from DB.DBA.SYS_USERS where U_ID = _owner and U_IS_ROLE = 0);
  if (_home is not null)
    _parent := WV.WIKI.DAV_HOME_CREATE (_uname);  
  else
    {
      _home := '/DAV/VAD/wiki/';
      _parent := DAV_HIDE_ERROR (DB.DBA.DAV_SEARCH_ID(_home, 'C'));
    }

  if (_parent < 0)
    WV.WIKI.APPSIGNAL (11001, 'Unable to find existing or create a new DAV collection for internal Wiki files', null);
-- Check if a cluster is already registered.
  if (exists (select * from WV.WIKI.CLUSTERS where ClusterName = _cname))
    {
  --dbg_obj_print ('7');

      if (signal_err = 1)
	WV.WIKI.APPSIGNAL (11001, 'Cluster "&ClusterName;" already exists',
				 vector ('ClusterName', _cname) );
      else
	return;
    }
-- Preparing main collection
  if (_src_col <> 0)
    {
      if (DB.DBA.DAV_SEARCH_PATH (_src_col, 'C') < 0)
	WV.WIKI.APPSIGNAL (11001, 'Invalid DAV collection ID "&ColId;"; can not create cluster "&ClusterName;"',
	  vector ('ColId', _src_col, 'ClusterName', _cname) );
      _main := _src_col;
    }
  else
    {
      _main := WV.WIKI.CREATEDAVCOLLECTION (_parent, _cname, _owner, _group);
    }
  --dbg_obj_print ('8');

  if (__proc_exists('DB.DBA.Versioning_DAV_SEARCH_ID'))
    {
      _histcol := WV.WIKI.CREATEDAVCOLLECTION (_main, 'VVC', _owner, _group);
      DB.DBA.DAV_SET_VERSIONING_CONTROL (DAV_SEARCH_PATH (_main, 'C'), NULL, 'A', 'dav', (select pwd_magic_calc (U_NAME, U_PWD, 1) from WS.WS.SYS_DAV_USER where U_ID = http_dav_uid() ));
    }
next:
    --dbg_obj_print ('9');

  _xmlcol := WV.WIKI.CREATEDAVCOLLECTION (_main, 'xml', _owner, _group);
  _attachcol := WV.WIKI.CREATEDAVCOLLECTION (_main, 'attach', _owner, _group);
  declare _cluster_id int;
  _cluster_id := WV.WIKI.NEWCLUSTERID();
  insert into WV.WIKI.CLUSTERS (ClusterId, ClusterName, ColId, ColHistoryId, ColXmlId, ColAttachId, AdminId, C_NEWS_ID)
    values (_cluster_id, _cname, _main, _histcol, _xmlcol, _attachcol, _owner, 'oWiki-' || _cname);
  for select RES_ID as _res_id, RES_FULL_PATH as _full_path,
    RES_CONTENT as _content,
    RES_COL as _col_id,
    RES_NAME as _name,
    RES_OWNER as _owner
    from WS.WS.SYS_DAV_RES 
    where RES_COL = _main and RES_NAME like '%.txt' and 
      not exists (select 1 from WV.WIKI.TOPIC where ResId = RES_ID)
  do {
    insert replacing WV.WIKI.TOPIC (TopicId, ClusterId, ResId, LocalName, LocalName2, Abstract, MailBox)
      values (WV.WIKI.NEWPLAINTOPICID(), _cluster_id, _res_id, 
      	subseq (_name, 0, length (_name) - 4), 
      	subseq (_name, 0, length (_name) - 4), 
	NULL,
	NULL);
    WV.WIKI.GETLOCK (_full_path, 'dav');
    WV.WIKI.UPLOADPAGE (_col_id, _name, blob_to_string (_content) || ' ',
       _uname, _cluster_id, 'dav');
--    DB.DBA.DAV_CHECKIN_INT (_full_path, null, null, 0);
    WV.WIKI.RELEASELOCK (_full_path, 'dav');
  }
  WV.WIKI.CREATEROLES (_cname);
  --dbg_obj_print ('11');

  if ( (_cname <> 'Main') and
       (_cname <> 'Doc'))
    {
      WV.WIKI.UPDATEGRANTS_FOR_RES_OR_COL (_cname, _main, 'C');
      WV.WIKI.CREATEINITIALPAGE ('ClusterSummary.txt', _main, _owner, 'Template');
      WV.WIKI.CREATEINITIALPAGE ('WelcomeVisitors.txt', _main, _owner, 'Template');
    }
  else
    {
     for select RES_NAME from WS.WS.SYS_DAV_RES where RES_FULL_PATH like '/DAV/VAD/wiki/' || _cname || '/%.txt' do 
       {
         WV.WIKI.CREATEINITIALPAGE (RES_NAME, _main, _owner, _cname);
       }
    }
	  
  WV.WIKI.SETCLUSTERPARAM (_cname, 'creator', _uname);
  --dbg_obj_print ('12');

}
;


create procedure WV.WIKI.UPLOADPAGE (
	in _col_id int,
	in _name varchar, 
	in _text any, 
	in _owner varchar, 
	in _cluster_id int:=0,
	in _user varchar:='dav')
{
  --dbg_obj_princ ('WV.WIKI.UPLOADPAGE ',  _col_id,  _name, _text , _owner,  _cluster_id ,_user);
  declare _res_id int;
  if (_cluster_id = 0)
    _cluster_id := (select ClusterId from WV.WIKI.CLUSTERS where ColId = _col_id);
  declare _perms, _path varchar;
  _path := WS.WS.COL_PATH(_col_id) || _name;
  _perms := WV.WIKI.GETDEFAULTPERMS (_cluster_id);
  connection_set ('HTTP_CLI_UID', _user);
  _res_id := DB.DBA.DAV_RES_UPLOAD (
     _path,
     _text,
     'text/plain',
     _perms, 
     _owner,
     'WikiUser', 
     'dav', 
     (select pwd_magic_calc (U_NAME, U_PWD, 1) from WS.WS.SYS_DAV_USER where U_ID = http_dav_uid()),
     coalesce ( (select Token from WV.WIKI.LOCKTOKEN where UserName = _user and ResPath = _path), 1));
     
   --dbg_obj_princ ('perms=', _perms, ' res= ', _res_id);
  return _res_id;
}
;

create procedure WV.WIKI.CREATEINITIALPAGE (in _page varchar, in _main int, in _owner_id int, in _templ_root varchar:='Main')
{
  whenever sqlstate '*' goto fin;
  declare _content, _type, _owner, _pwd varchar;
  select pwd_magic_calc (U_NAME, U_PASSWORD, 1), U_NAME into _pwd, _owner from DB.DBA.SYS_USERS where U_ID = _owner_id and U_IS_ROLE = 0;

  declare _template_collection varchar;
  _template_collection := '/DAV/VAD/wiki/' || _templ_root || '/';
  if (0 < DB.DBA.DAV_RES_CONTENT (_template_collection || _page, _content, _type, _owner, _pwd))
    {
      declare _fullpath varchar;
      _fullpath := DB.DBA.DAV_SEARCH_PATH (_main, 'C') || _page;
--      WV.WIKI.GETLOCK (_fullpath, 'dav');
      WV.WIKI.UPLOADPAGE (_main, _page, _content, _owner);
--      DB.DBA.DAV_CHECKIN_INT (_fullpath, null, null, 0);
--      WV.WIKI.RELEASELOCK (_fullpath, 'dav');
    }
fin:
	;
}
;

create procedure WV.WIKI.DELETETOPIC (in _id integer)
{
  --dbg_obj_princ ('DELETETOPIC: ', _id);
  delete from WV.WIKI.TOPIC where TopicId = _id;
  if (__proc_exists ('DB.DBA.WA_NEW_RM'))
     WA_NEW_WIKI_RM (_id);
}
;

create procedure WV.WIKI.DROPCLUSTERCONTENT (in _cid int)
{
  declare _topic WV.WIKI.TOPICINFO;
  _topic := WV.WIKI.TOPICINFO ();
  _topic.ti_cluster_id := _cid;
  _topic.ti_fill_cluster_by_id ();

  declare _dir_list, _pwd any;
  _pwd :=  (select pwd_magic_calc (U_NAME, U_PWD, 1) from WS.WS.SYS_DAV_USER where U_ID = http_dav_uid());
  _dir_list := DAV_DIR_LIST (DB.DBA.DAV_SEARCH_PATH (_topic.ti_col_id, 'C'), 0, 'dav', _pwd);

  foreach (any _file in _dir_list) do {
	if ( (aref (_file, 1) = 'R') or (aref (_file, 1) = 'r') )
		DB.DBA.DAV_DELETE ( aref (_file, 0), 0, 'dav', _pwd);
  }
  delete from WV.WIKI.TOPIC where ClusterId = _cid;
}
;

create procedure WV.WIKI.DELETECLUSTER (in _cid varchar)
{
  if (exists (select 1 from WV.WIKI.TOPIC where ClusterId = _cid))
    WV.WIKI.APPSIGNAL (11001, 'Cluster "&ClusterName;" is not empty; delete all topics first',
      vector ('ClusterName', coalesce ((select ClusterName from WV.WIKI.CLUSTERS where ClusterId = _cid), cast (_cid as varchar))) );
  delete from WV.WIKI.CLUSTERS where ClusterId = _cid;
}
;

create procedure WV.WIKI.LOADCOLLECTIONFROMFILES (in _dirfullpath varchar, in _col_id integer, in _make_result_names integer default 1,  in _user varchar default 'Wiki', in _group varchar default 'WikiAdmin')
{
  declare _dirlist, _filelist any;
  declare _diridx, _fileidx integer;
  declare _fname varchar;
  declare Directory, "File Name" varchar;
  declare "Parent Id", "Res Id" integer;
  if (_make_result_names)
    result_names (Directory, "File Name", "Parent Id", "Res Id");
  _filelist := sys_dirlist (_dirfullpath, 1);
  _fileidx := 0;
  while (_fileidx < length (_filelist))
    {
      declare _filename varchar;
      declare _rid integer;
      declare _text any;
      _filename := aref (_filelist, _fileidx);
      _text := file_to_string_output (concat (_dirfullpath, '/', _filename));
      _rid := DB.DBA.DAV_RES_UPLOAD_STRSES (concat (WS.WS.COL_PATH(_col_id), _filename), _text, 'text/plain', '110100100R', _user, _group, 'Wiki', null);
      result (_dirfullpath, _filename, _col_id, _rid);
      _fileidx := _fileidx + 1;
      commit work;
    }
  _filelist := sys_dirlist (_dirfullpath, 0);
  _fileidx := 0;
  while (_fileidx < length (_filelist))
    {
      declare _filename varchar;
      _filename := aref (_filelist, _fileidx);
      WV.WIKI.LOADSUBDIRECTORY (_dirfullpath, _col_id, _filename, 0, _user, _group);
      result (_dirfullpath, _filename, _col_id, NULL);
      _fileidx := _fileidx + 1;
      commit work;
    }
}
;


create procedure WV.WIKI.LOADSUBDIRECTORY (in _parent_path varchar, in _parent_col_id integer, in _dirname varchar, in _make_result_names integer default 1, in _user varchar default 'Wiki', in _group varchar default 'WikiAdmin')
{
  declare _rid integer;
  if (('.' = _dirname) or ('..' = _dirname))
    return;
  declare Directory, "File Name" varchar;
  declare "Parent Id", "Res Id" integer;
  if (_make_result_names)
    result_names (Directory, "File Name", "Parent Id", "Res Id");
  _rid := DB.DBA.DAV_COL_CREATE(concat (WS.WS.COL_PATH(_parent_col_id), _dirname, '/'), '110100100R', _user, _group, 'Wiki', null);
  result (_parent_path, _dirname, _parent_col_id, _rid);
  if (_rid < 0)
    return;
  WV.WIKI.LOADCOLLECTIONFROMFILES (concat (_parent_path, '/', _dirname), _rid, 0, _user, _group);
}
;  


create procedure WV.WIKI.LOADCLUSTERFROMFILES (in _dirfullpath varchar, in _cluster varchar, in _user varchar default 'Wiki', in _group varchar default 'WikiAdmin')
{
  declare _topic WV.WIKI.TOPICINFO;
  _topic := WV.WIKI.TOPICINFO();
  _topic.ti_cluster_name := _cluster;
  _topic.ti_fill_cluster_by_name();
  WV.WIKI.LOADCOLLECTIONFROMFILES (_dirfullpath, _topic.ti_col_id, 1, _user, _group);
}
;


-- Utils


create function WV.WIKI.SINGULARPLURAL (in _src varchar)
{ -- Converts an English noun between singular and plural form.
  declare _src_len integer;
  declare _suf varchar;
  _src_len := length (_src);
  if (_src_len < 4)
    return _src;
  _suf := right (_src, 3);
  if (_suf = 'SES')
    return concat (left (_src, _src_len - 2), 'S');
  if (_suf = 'ses')
    return concat (left (_src, _src_len - 2), 'ses');
  _suf := right (_src, 2);
  if (_suf = 'SS')
    return concat (left (_src, _src_len - 2), 'SES');
  if (_suf = 'ss')
    return concat (left (_src, _src_len - 2), 'ses');
  _suf := right (_src, 1);
  if (_suf = 'S' or _suf = 's')
    return left (_src, _src_len - 1);
  if (upper(_suf) = _suf)
    return concat (_src, 'S');
  return concat (_src, 's');
}
;

create function WV.WIKI.FORCEDWIKIWORD (in _src varchar)
{
  -- converts "hello world" to "HelloWorld"
  return _src;
}
;

create function WV.WIKI.FILENAMETOWIKINAME (in _src varchar)
{ -- Converts file name to WikiName of the topic.
  declare _src_len integer;
  declare _suf varchar;
  _src_len := length (_src);
  if (_src_len < 4)
    return _src;
  _suf := right (_src, 4);
  if (_suf = '.txt')
    return left (_src, _src_len - 4);
  if (_suf = '.TXT')
    return left (_src, _src_len - 4);
  if (strchr (_src, '.'))
    WV.WIKI.APPSIGNAL (11001, 'Resource name "&ResName;" cannot be converted to Wiki page name (must have .txt extension or no extension at all)',
      vector ('ResName', _src) );
  return _src;
}
;

create procedure WV.WIKI.APPSIGNAL (in _errno integer, in _text varchar, in _data any)
{ -- Signals formatted error message.
-- _text may contain substrings like &ParameterName;
-- _data is a vector of parameter names and values.
  declare _val any;
  declare _res varchar;
  declare _ctr integer;
-- TODO: search in AppErrors.
  _res := _text;
  _ctr := length (_data);  
  while (_ctr > 1)
    {
      _ctr := _ctr - 2;
      _val := aref (_data, _ctr+1);
      if (_val is null)
        _val := '(unknown)';
      else
        _val := cast (_val as varchar);
      _res := replace (_res, concat ('&', aref (_data, _ctr), ';'), _val);
    }
  signal ('42WV9', _res);
}
;

create procedure WV.WIKI.GETATTCOLID (in _topic WV.WIKI.TOPICINFO)
{
  declare _col_path varchar;
  declare _attachment_col_id int;
  _col_path := DB.DBA.DAV_SEARCH_PATH (_topic.ti_col_id, 'C');
  _attachment_col_id := DB.DBA.DAV_SEARCH_ID ( _col_path || _topic.ti_local_name || '/', 'C');

  if (_attachment_col_id < 0)
	_attachment_col_id := DB.DBA.DAV_SEARCH_ID ( _col_path || _topic.ti_local_name_2 || '/', 'C');
  return _attachment_col_id;
}
;

create procedure WV.WIKI.ATTACH2 (in _uid int, in _filename varchar, in _type varchar, in id integer, inout _text any, in comment varchar)
{
  declare _attachment_col_id int;
  declare _topic WV.WIKI.TOPICINFO;
  _topic := WV.WIKI.TOPICINFO ();
  _topic.ti_id := id;
  _topic.ti_find_metadata_by_id ();

  _attachment_col_id := WV.WIKI.GETATTCOLID (_topic);
  if (_attachment_col_id < 0)
    _attachment_col_id := WV.WIKI.CREATEDAVCOLLECTION (_topic.ti_col_id, _topic.ti_local_name, _uid,  WV.WIKI.WIKIUSERGID());
  declare _full_path varchar;
  _full_path := WS.WS.COL_PATH (_attachment_col_id) || _filename;
  
  declare _path, _user varchar;
  _path := DB.DBA.DAV_SEARCH_PATH (_topic.ti_res_id, 'R');
  _user := (select U_NAME from DB.DBA.SYS_USERS where U_ID = _uid);
  DB.DBA.DAV_RES_UPLOAD (_full_path, _text, _type, '110110000R', 
    _uid, 'WikiUser', 'dav', (select pwd_magic_calc (U_NAME, U_PWD, 1) from WS.WS.SYS_DAV_USER where U_ID = http_dav_uid()),
    coalesce ( (select Token from WV.WIKI.LOCKTOKEN where UserName = _user and ResPath = _path), 1));
  insert replacing WV.WIKI.ATTACHMENTINFONEW values (_filename, _full_path, comment);
  DB.DBA.DAV_PROP_SET_INT(_full_path, 'oWiki:belongs-to', _path, null, null, 0, 1, 1);
  DB.DBA.DAV_PROP_SET_INT(_full_path, 'oWiki:md5', md5(cast (_text as varbinary)), null, null, 0, 1, 1);
  commit work;
}
;

create procedure WV.WIKI.ATTACHMENTACTION (
  in _uid integer, 
  in _topic_id integer,
  in _attachment_name varchar,
  in _cmd varchar)
{
  declare _topic WV.WIKI.TOPICINFO;
  declare _text varchar;
  _topic := WV.WIKI.TOPICINFO ();
  _topic.ti_id := _topic_id;
  _topic.ti_find_metadata_by_id ();

  declare _attachment_col_id int;
  _attachment_col_id := WV.WIKI.GETATTCOLID (_topic);
  DB.DBA.DAV_DELETE (concat (WS.WS.COL_PATH(_attachment_col_id), _attachment_name), 1, 'dav', (select pwd_magic_calc (U_NAME, U_PWD, 1) from WS.WS.SYS_DAV_USER where U_ID = http_dav_uid()));
}
;

WV.Wiki.EXEC_41000_I('drop procedure "WV"."Wiki"."CheckReadAccess"')
;

create function WV.WIKI.MAXPARENTDEPTH ()
{
  return 25;
}
;

create procedure WV.WIKI.CHECKPARENT (in _topic_id int, in _parent int, in _depth int)
{
  if (_parent = 0)
    return 0;
  if ((_depth > WV.WIKI.MAXPARENTDEPTH()) or (_parent = _topic_id))
    {
	return -1;
    }
  return WV.WIKI.CHECKPARENT (_topic_id, (select ParentId from  WV.WIKI.TOPIC where TopicId = _parent),  _depth + 1);
}
;

create procedure WV.WIKI.TOPICSETPARENT (in _topic_id int, in _parent int)
{
  if (WV.WIKI.CHECKPARENT (_topic_id, _parent, 0) = -1)
    {
	declare _topic WV.WIKI.TOPICINFO;
	_topic := WV.WIKI.TOPICINFO ();
	_topic.ti_id := _topic_id;
	_topic.ti_find_metadata_by_id ();
  	return 'This topic can not be set as parent of ' || _topic.ti_cluster_name || '.' || _topic.ti_local_name;
    }
  update WV.WIKI.TOPIC set ParentId = _parent where TopicId = _topic_id;
  return null;
}
;

create procedure WV.WIKI.ISWIKIWORD (in _nm varchar)
{
  declare _res varchar;
  _nm := trim (_nm);
  _res := regexp_match ('[A-Z]+[a-z]+[A-Z]+[A-Za-z0-9]*', _nm);
  if (_res is null) 
	return 0;
  if (_res <> _nm) 
	return 0;
  return 1;
}
;

create procedure WV.WIKI.RENAMETOPIC (in _topic WV.WIKI.TOPICINFO,
	in _user varchar, 
	in new_cluster int, 
	in new_name varchar)
{
  new_name := trim (new_name);
  if (new_name = '')
	signal ('XXXXX', 'Invalid topic name');
  if (WV.WIKI.ISWIKIWORD (new_name) <> 1)
	signal ('XXXXX', new_name || ' is not WikiWord');

  declare _from, _to varchar;
  _from := DB.DBA.DAV_SEARCH_PATH (_topic.ti_res_id, 'R');
  _to := DB.DBA.DAV_SEARCH_PATH ( (select ColId from WV.WIKI.CLUSTERS where ClusterId = new_cluster) , 'C') || new_name || '.txt';
 
  declare _res int;
  _res := DB.DBA.DAV_MOVE_INT (_from, _to, 1, 
    'dav', (select pwd_magic_calc (U_NAME, U_PWD, 1) from WS.WS.SYS_DAV_USER where U_ID = http_dav_uid()), 1, 
    coalesce (WV.WIKI.GET_LOCKTOKEN (_topic.ti_res_id), 1));
  if (DAV_HIDE_ERROR (_res) is null)
    WV.WIKI.APPSIGNAL (11002, '&Topic; can not be moved to &NewTopic; due to DAV_MOVE fail: &Result;',
       vector ('Topic', _topic.ti_local_name, 'NewTopic', new_name, 'Result', _res));
  commit work;
}
;

create procedure WV.WIKI.GETFULLDAVPATH (in col_id int, in _res_id int, in local_name varchar)
{
  declare _res_path varchar;
  _res_path := DB.DBA.DAV_SEARCH_PATH (_res_id, 'R');
  if (_res_path <> -1)
    return _res_path;
  return WS.WS.COL_PATH (col_id) || local_name || '.txt';
}
;

create function WV.WIKI.CHECKREADACCESS (in _u_id integer,
					       in res_id integer,
					       in res_cluster_id integer,
					       in res_col_id integer,
					       in error_message varchar := NULL) returns integer
{
  return WV.WIKI.CHECKACCESS (_u_id, res_id, res_cluster_id, res_col_id, 0, error_message);
}
;

create function WV.WIKI.CHECKWRITEACCESS (in _u_id integer,
					       in res_id integer,
					       in res_cluster_id integer,
					       in res_col_id integer,
					       in error_message varchar := NULL) returns integer
{
  return WV.WIKI.CHECKACCESS (_u_id, res_id, res_cluster_id, res_col_id, 1, error_message);
}
;

-- returns lock token, or null
create function WV.WIKI.GET_LOCKTOKEN (in res_id int)
{
  declare res_path varchar;
  res_path := DB.DBA.DAV_SEARCH_PATH (res_id, 'R');
  --dbg_obj_print ((select Token from WV.WIKI.LOCKTOKEN where ResPath = res_path));
  if (isstring (res_path))
    return (select Token from WV.WIKI.LOCKTOKEN where ResPath = res_path);
  return NULL;
}
; 
      

create function WV.WIKI.GETLOCK (in _path varchar, in _uname varchar) returns integer
{
  -- while DAV_LOCK does not work...
  declare _token varchar;
  declare _res_id int;
  _res_id := DAV_SEARCH_ID (_path, 'R');
  _token := (select Token from WV.WIKI.LOCKTOKEN
  	where ResPath = _path);
  declare _type varchar;
  declare _res int;
  _type := 'R';
  if ( 0 < (_res := DB.DBA.DAV_IS_LOCKED_INT (_res_id, _type, _token) ))
    return _res;
  _token := DB.DBA.DAV_LOCK (_path, 'R', 'R', null, _uname, null, null, WV.WIKI.LOCKEXPIRATION() , _uname,
	(select pwd_magic_calc(U_NAME, U_PASSWORD, 1) from DB.DBA.SYS_USERS where U_NAME = _uname));
  if (isstring (_token))
    {
      insert replacing WV.WIKI.LOCKTOKEN (UserName, ResPath, Token)
        values (_uname, _path, _token);
      return 0;
    }
  if (_token = -35)
    return 0; -- already locked.
  return _token;
}
;

create procedure WV.WIKI.RELEASELOCK (in _path varchar, in _uname varchar)
{
  -- while DAV_LOCK does not work
  if (isstring (_path))
    {
      declare _token varchar;
      _token := (select Token from WV.WIKI.LOCKTOKEN 
      	where ResPath = _path);
      if (_token is not null)
        {
	  if (DAV_HIDE_ERROR (DB.DBA.DAV_UNLOCK (_path, _token, _uname, 
		(select pwd_magic_calc(U_NAME, U_PASSWORD, 1) from DB.DBA.SYS_USERS where U_NAME = _uname) )) is not null)
	   {
	     delete from WV.WIKI.LOCKTOKEN where ResPath = _path;
	   }
	}
    }
}
;

create function WV.WIKI.LOCKEXPIRATION ()
{
  return 60 * 60; -- 1 hour
}
;

create function WV.WIKI.PARAM (in _user any, in _param varchar, in _defval any:=null) returns any
{
  declare _uid int;
  if (isstring (_user))
    _uid := (select U_ID from DB.DBA.SYS_USERS where U_NAME = _user);
  else
    _uid := _user;

  for select Value from WV.WIKI.USERSETTINGS 
	       where ParamName = _param 
	       and UserId = _uid do
    {
      return Value;
    }
  return _defval;
}
;


create function WV.WIKI.GETCLUSTERID (in _cluster any) returns int
{
  declare _cl_id int;
  if (isstring (_cluster))
    _cl_id := (select ClusterId from WV.WIKI.CLUSTERS where ClusterName = _cluster);
  else
    _cl_id := _cluster;
  if (_cl_id is null)
    signal ('XXXX', 'Unkown cluster' || _cluster);
  return _cl_id;
}
;

create function WV.WIKI.GETCLUSTERNAME (in _cluster int) returns varchar
{
  return (select ClusterName from WV.WIKI.CLUSTERS where ClusterId = _cluster);
}
;


create function WV.WIKI.CLUSTERPARAM (in _cluster any, in _param varchar, in _defval any:=null) returns any
{
  --dbg_obj_princ ('CLUSTERPARAM ', _cluster, ' ', _param, ' ', _defval);
  for select Value from WV.WIKI.CLUSTERSETTINGS 
	       where ParamName = _param 
	       and ClusterId = WV.WIKI.GETCLUSTERID (_cluster)
  do 
    {
      return Value;
    }
  return _defval;
}
;

grant execute on WV.WIKI.CLUSTERPARAM to public
;

xpf_extension ('http://www.openlinksw.com/Virtuoso/WikiV/:ClusterParam', 'WV.WIKI.CLUSTERPARAM')
;

create procedure WV.WIKI.SETPARAM (in _user any, in _param varchar, in _val any)
{
  declare _uid int;
  if (isstring (_user))
    _uid := (select U_ID from DB.DBA.SYS_USERS where U_NAME = _user);
  else
    _uid := _user;

  insert replacing WV.WIKI.USERSETTINGS values (_uid, _param, _val);
}
;

create procedure WV.WIKI.SETCLUSTERPARAM (in _cluster any, in _param varchar, in _val any)
{
  insert replacing WV.WIKI.CLUSTERSETTINGS values ( WV.WIKI.GETCLUSTERID (_cluster), _param, _val);
}
;

create function WV.WIKI.MAKECATEGORYNAME (in _name varchar) returns varchar
{
  return ucase (subseq (_name, 0, 1)) || lcase (subseq (_name, 1));
}
;

create function WV.WIKI.MAKECATEGORYSHORTNAME (in _name varchar) returns varchar
{
  return lcase (subseq (_name, 8));
}
;


create function WV.WIKI.TOUCHCATEGORY (in _cluster_id int, in fullname varchar, in is_pub int) returns varchar
{
  declare _catname varchar;
  _catname := WV.WIKI.MAKECATEGORYSHORTNAME (fullname);
			       
  if (not exists (select 1 from WV.WIKI.CATEGORY where lcase (CategoryName) = lcase (fullname) 
	and _cluster_id = ClusterId))
    {
       insert into WV.WIKI.CATEGORY (CategoryId,
					  ClusterId,
					  IsDelIcioUsPub,
					  CategoryName,
					  ShortName)
	values (WV.WIKI.NEWPLAINTOPICID(),
		_cluster_id,
		is_pub,
		fullname,
		_catname);
    }
  else if (is_pub)
    WV.WIKI.DELICIOUSSYNCCATEGORY (fullname);
  return _catname;
}
;
  

create function WV.WIKI.DIUCATEGORYLINK (
  in _tag varchar) returns any
{ 
  -- creates local category if needed, returns wikiword				       
  declare _cat varchar;
  declare _c_id int;
  declare _c_col_id, _owner int;
  _c_id := connection_get ('ClusterId');
  _c_col_id := connection_get ('ColId');
  _owner := connection_get ('Owner');
  _cat := WV.WIKI.TAG_TO_CATEGORY (_tag, _c_id);
  if (not exists (select * from WV.WIKI.TOPIC where LocalName = _cat and ClusterId = _c_id))
    {
      WV.WIKI.UPLOADPAGE (_c_col_id, _cat || '.txt', 'Imported from del.icio.us\nCategoryCategory', _owner);
--      DB.DBA.DAV_CHECKIN_INT (DB.DBA.DAV_SEARCH_PATH (_c_col_id, 'C') ||  _cat || '.txt', null, null, 0);
    }
  return _cat;
}
;

grant execute on WV.WIKI.DIUCATEGORYLINK to public
;

xpf_extension ('http://www.openlinksw.com/Virtuoso/WikiV/:DIUCategoryLink', 'WV.WIKI.DIUCATEGORYLINK')
;


-- should syncs all posts with del.icio.us in the category _category
-- but del.icio.us does not allow more frequent updates than 1 time per
-- second... so the implementatiion is still uncertan
create procedure WV.WIKI.DELICIOUSSYNCCATEGORY (in _category varchar)
{
  update WV.WIKI.CATEGORY set IsDelIcioUsPub = 1 where CategoryName = _category;
}
;


create procedure WV.WIKI.DELICIOUSSIGNAL (in _cluster any, in _err varchar)
{
  WV.WIKI.SETCLUSTERPARAM (_cluster, 'DelIcioUsLastError',
				 vector (now (),
					 _err));
  return xtree_doc ('<div class="wiki-error">' || _err || '</div>');
}
;
	
create function WV.WIKI.DELICIOUSUPDATEFUNCTION (in _is_full int, in last_date datetime)
{
  ;
}
;

create procedure WV.WIKI.DELICIOUSSYNC (in _cluster int, in _user varchar)
{
  --dbg_obj_princ ('WV.WIKI.DELICIOUSSYNC: ', _cluster, ' ', _user);
  declare _digest varchar;
  _digest := WV.WIKI.CLUSTERPARAM (_cluster , 'delicious_digest');
  if (_digest is null)
    return WV.WIKI.DELICIOUSSIGNAL (_cluster, 'del.icio.us integration is not configured');

  declare _hdr, _doc, _res any; 
  --dbg_obj_princ ('http://del.icio.us/api/tags/get?', 'POST', sprintf ('Content-Type: text/xml\r\nDepth: 1\r\nAuthorization: Basic %s', _digest));
  _res := http_get('http://del.icio.us/api/tags/get?', _hdr, 'POST', sprintf ('Content-Type: text/xml\r\nDepth: 1\r\nAuthorization: Basic %s', _digest));
  --dbg_obj_print (_res);
  _doc := xtree_doc (_res, 2);

  connection_set ('ClusterId', _cluster);
  connection_set ('ColId', (select ColId from WV.WIKI.CLUSTERS where ClusterId = _cluster));
  connection_set ('Auth', _user);
  connection_set ('Owner', (select U_ID from DB.DBA.SYS_USERS where U_NAME = _user));

  _res := xquery_eval ('
<div xmlns:wv="http://www.openlinksw.com/Virtuoso/WikiV/">
<div class="wiki_container">
  <ul>
  {
    for \$t in node()/tag[@tag != \'system:infiled\']
    order by \$t
    return 
	<li>{ wv:DIUCategoryLink (string(\$t/@tag)) }</li> 
  }
  </ul>
</div></div>', _doc);
  
  WV.WIKI.SETCLUSTERPARAM (_cluster, 'delicious_last_update', now());
  return _res;
}
;

create method ti_fill_url () for WV.WIKI.TOPICINFO
{
  declare _home varchar;
  _home := WV.WIKI.CLUSTERPARAM (self.ti_cluster_name, 'home', '/wiki/main');
  self.ti_url := sprintf ('%s/%s',
			  _home,
			  WV.WIKI.READONLYWIKIWORDLINK (self.ti_cluster_name, self.ti_local_name));
  return self.ti_url;
}
;

create procedure WV.WIKI.MAKECATEGORYNAMELIST (in _cluster int, in _category_names any)
{
  declare _res varchar;
  _res := null;
  declare idx int;
  idx := 0;
  while (idx < length (_category_names))
    {
      declare _fullname varchar;
      _fullname := aref (_category_names, idx);
      WV.WIKI.TOUCHCATEGORY (_cluster, _fullname, 1);
      if (_res is null)
        _res := WV.WIKI.CATEGORY_TO_TAG (_fullname);
      else
	_res := _res || '%20' || WV.WIKI.CATEGORY_TO_TAG (_fullname);
      idx := idx + 1;
    }
  return _res;
}
;

create function WV.WIKI.MAKEDELICIOUSDATESTAMP (in dt datetime)
{
  declare _parsed_dt any;
  _parsed_dt := split_and_decode (cast (dt as varchar), 0, '\0\0 ');
  if (length (_parsed_dt) > 1)
    {
      return 
	replace (aref (_parsed_dt, 0), '.', '-') 
	|| 'T' 
	|| subseq (aref (_parsed_dt, 1), 0, 8)
	|| 'Z';
    }
  return null;
}
;
	

create procedure WV.WIKI.DELICIOUSPUBLISH (in _topic_id int, in _category_names any)
{
  declare _topic WV.WIKI.TOPICINFO;
  _topic := WV.WIKI.TOPICINFO ();
  _topic.ti_id := _topic_id;
  _topic.ti_find_metadata_by_id ();

  declare exit handler for sqlstate 'HT*', sqlstate '2E*' {
    rollback work;
    return WV.WIKI.DELICIOUSSIGNAL (_topic.ti_cluster_id, 'del.icio.us connection error');
  };

  declare _digest varchar;
  _digest := WV.WIKI.CLUSTERPARAM (_topic.ti_cluster_id , 'delicious_digest');
  if (_digest is null)
    return WV.WIKI.DELICIOUSSIGNAL (_topic.ti_cluster_id, 'del.icio.us integration is not configured');

  declare _func_post varchar;
  commit work;
  _func_post := sprintf ('http://del.icio.us/api/posts/add?&url=%s&description=%s&extended=&tags=%s&dt=%s',
			 _topic.ti_fill_url (),
			 _topic.ti_local_name, -- temporary solution
			 WV.WIKI.MAKECATEGORYNAMELIST (_topic.ti_cluster_id, _category_names),
			 WV.WIKI.MAKEDELICIOUSDATESTAMP (now()));
  declare _hdr, _doc, _res any; 
  _res := http_get(_func_post, _hdr, 'POST', sprintf ('Content-Type: text/xml\r\nDepth: 1\r\nAuthorization: Basic %s', _digest));
  return _res;
}
;

-- parameter="value" -> vector (parameter, value)
-- do not signal error, assume string is OK
create function WV.WIKI.PARSEPARAM (in arg varchar) returns any
{
  declare _res any;
  _res := split_and_decode (arg, 0, '\0\0=');
  aset (_res, 1, subseq ( subseq ( aref (_res, 1), 
				   0, length (aref (_res, 1)) - 1),
			  1));
  return _res;
}
;
			  
create function WV.WIKI.PARSEMACROARGS (in args varchar, in flatten int := 0) returns any
{
  declare _args, _res any;
  vectorbld_init(_args);
  declare x varchar;
  x := regexp_substr ('[A-Za-z]*=\"[^"]*\"', args, 0);
  while (x is not null)
    {
      args := subseq (args, length (x));
      vectorbld_acc (_args, x);
      x := regexp_substr ('[A-Za-z]*=\"[^"]*\"', args, 0);
    }
  vectorbld_final (_args);
  --dbg_obj_print (_args);
  declare _len int;
  
  _len := length (_args);
  _res := make_array (_len, 'any');
  declare idx int;
  idx := 0;
  while (idx < length (_args))
    {
      aset (_res, idx, WV.WIKI.PARSEPARAM (aref (_args, idx)));
      idx := idx +1;
    }
  if (flatten)
    {
      declare _flatten_res any;
      _flatten_res := make_array (_len * 2, 'any');
      for (idx := 0; idx < _len ; idx:=idx+1)
	{
	  _flatten_res[idx*2] := _res[idx][0];
	  _flatten_res[idx*2+1] := _res[idx][1];
	}
      return _flatten_res;
    }
  return _res;
}
;

create function WV.WIKI.GETMACROPARAM (in params any, in name varchar, in defval varchar:='') returns varchar
{
  declare _idx int;
  declare _name varchar;
  _name := lcase (name);
  while (_idx < length (params))
    {
      if (_name = lcase (aref ( aref (params, _idx), 0)))
	return aref ( aref (params, _idx), 1);
    _idx := _idx + 1;
    }
  return defval;
}
;

create function WV.WIKI.CHECKACCESS (in _u_id integer,
					   in _res_id integer,
					   in res_cluster_id integer,
					   in _res_col_id integer,
					   in is_write int,
					   in error_message varchar:= NULL) returns integer
{
  declare acc_str varchar;
  acc_str := (case when (is_write) then '_1_' else '1__' end);

  declare rc integer;
  declare _pwd, _uname varchar;
  declare exit handler for not found {
    --dbg_obj_princ (__SQL_STATE, __SQL_MESSAGE);
    goto err;
  }
  ;
  select pwd_magic_calc (U_NAME, U_PASSWORD, 1), U_NAME into _pwd, _uname from DB.DBA.SYS_USERS where U_ID = _u_id and U_IS_ROLE = 0;
  --dbg_obj_princ ('DAV_AUTHENTICATE: ', _res_id, '/', _res_col_id, '  ', acc_str, ' ', _uname, ' ', _pwd);
  if (_res_id <> 0)
    rc := DAV_AUTHENTICATE (_res_id, 'R', acc_str, _uname, _pwd);
  else
    rc := DAV_AUTHENTICATE (_res_col_id, 'C', acc_str, _uname, _pwd);
  if (rc >= 0)
    return 1;
err:    
  if (is_write)
    WV.WIKI.APPSIGNAL (11002, coalesce (error_message, 'Write access to the resource has not been granted'), vector());
  else 
    WV.WIKI.APPSIGNAL (11002, coalesce (error_message, 'Read access to the resource has not been granted'), vector());
}
;


create function WV.WIKI.GETDEFAULTPERMS (in _cluster_id int) returns varchar
{
  if (exists (select 1 from WV.WIKI.CLUSTERS
			where ClusterId = _cluster_id
			and ( ClusterName = 'Main'
			or ClusterName = 'Doc')))
    return '111101101RM';
  return '110000000RM';

  declare perms varchar;
  declare model int;
  model := connection_get ('WikiMemberModel');
  if (model is null)
    model := coalesce ( (select WAI_MEMBER_MODEL  from WA_INSTANCE 
		       where WAI_TYPE_NAME = 'oWiki' 
		       and (WAI_INST as wa_wikiv).cluster_id = _cluster_id), 0);
  if (model = 0) -- Open
    return '110110110R';
  -- Closed, Invitation Based, Approval Based
  -- all these model are managed by ACLs.
  return '110000000R';
}
;
  

create procedure WV.WIKI.ADDHISTORYITEM (in _topic WV.WIKI.ADDHISTORYITEM, 
					       in _filename varchar, 
					       in _action varchar, 
					       in _context varchar,
					       in _user varchar)
{
  ;
}
;
					       
create procedure WV.WIKI.PRINTLENGTH (in sz int)
{
  declare sz_postfix any;
  declare offs int;

  offs := 0;

  while (sz > 9999) {
    sz :=  sz / 1024;
    offs := offs + 1;
  }   
  sz := floor (sz);

  return cast (sz as varchar) || aref (vector ('b','K','M','G','T'), offs);
}
;

create procedure WV.WIKI.ADDLINK (in _topic WV.WIKI.TOPICINFO, 
	in _type varchar,
	in _uid varchar,
	in _filename varchar,
	in _user varchar)
{
  declare _link varchar;
  if (_type not like 'image/%')
	  _link  := '<a href="%ATTACHURLPATH%/' || _filename || '" style="wikiautogen">' || _filename  || '</a>';
  else
	  _link  := '<img src="%ATTACHURLPATH%/' || _filename || '" style="wikiautogen"/>';
  declare _path, _user varchar;
  _path :=DB.DBA.DAV_SEARCH_PATH (_topic.ti_res_id, 'R');
  _user := (select U_NAME from DB.DBA.SYS_USERS where U_ID = _uid);
  if (0 < WV.WIKI.GETLOCK (_path, _user))
    WV.WIKI.APPSIGNAL (11001, 'The resource &path; is locked', vector ('path', _path));
  connection_set ('HTTP_CLI_UID', _user);
  DB.DBA.DAV_RES_UPLOAD (_path,
		cast (_topic.ti_text as varchar) || '\n   * ' || _link , 
		'text/plain', 
		'110100100R', 
		_uid, 
		'WikiUser', 
		'dav', (select pwd_magic_calc (U_NAME, U_PWD, 1) from WS.WS.SYS_DAV_USER where U_ID = http_dav_uid()),
		coalesce ( (select Token from WV.WIKI.LOCKTOKEN where UserName = _user and ResPath = _path), 1));
--  DB.DBA.DAV_CHECKIN_INT (_path, null, null, 0);
}
;  

create procedure WV.WIKI.NORMALIZETOWIKIWORD (in _name varchar)
{
  if (length (_name) = 0)
	return _name;
  if (length (_name) = 1)
	return ucase (_name);
  return ucase ( subseq (_name, 0, 1) ) || subseq (_name, 1);
}
;

create procedure WV.WIKI.CHECKWIKIWORD (in _name varchar)
{
  if (length (_name) = 0)
   WV.WIKI.APPSIGNAL (11001, '"&ResName;" is not WikiWord', vector ('ResName', _name) );
  return _name;
}
;

create procedure WV.WIKI.GETCOLLECTIONS (in _path varchar, in recursive int:= 0)
{
  declare _dir_list, _res any;
  _dir_list := DAV_DIR_LIST (_path, 0, 'dav', (select pwd_magic_calc (U_NAME, U_PWD, 1) from WS.WS.SYS_DAV_USER where U_ID = http_dav_uid())); 
  _res := vector ();
  if (isarray (_dir_list))
    {
      foreach (any _dir in _dir_list) do {
	if (ucase (aref (_dir, 1)) = 'C')
	  {
	    declare _paths any;
	    _paths := split_and_decode (aref (_dir, 0), 0, '\0\0/');
	    _res := vector_concat (_res, vector (aref (_paths, length (_paths) -2) ));
	  }
      }
      return _res;
    }
  else
    return vector ();
}
;

create procedure WV.WIKI.DELETEATTACHMENTLINKS (
	in _topic WV.WIKI.TOPICINFO,
	in _uid int,
	in _attachment varchar)
{
  declare _path, _user varchar;
  _path := DB.DBA.DAV_SEARCH_PATH (_topic.ti_res_id, 'R');
  _user := (select U_NAME from DB.DBA.SYS_USERS where U_ID = _uid);
  DB.DBA.DAV_RES_UPLOAD (DB.DBA.DAV_SEARCH_PATH (_topic.ti_res_id, 'R'),
		WV.WIKI.DELETEATTACHMENTLINKS2 (_topic.ti_text, _attachment),
		'text/plain', 
		'110100100R', 
		_user, 
		'WikiUser', 
		'dav', (select pwd_magic_calc (U_NAME, U_PWD, 1) from WS.WS.SYS_DAV_USER where U_ID = http_dav_uid()),
		coalesce ( (select Token from WV.WIKI.LOCKTOKEN where UserName = _user and ResPath = _path), 1));
}
;

create procedure WV.WIKI.DELETEATTACHMENTLINKS2 (
	in _topic_text varchar,
	in _attachment varchar)
{
  declare _link, _link2 varchar;
  _link := '<[a|A]\\s+href="%ATTACHURLPATH%/' || _attachment || '"\\s+style="wikiautogen"[^>]*>.*</[a|A]>';
  _link2 := '<[i|I][m|M][g|G]\\s+src="%ATTACHURLPATH%/' || _attachment || '"\\s+style="wikiautogen"[^/>]*/>';
  return regexp_replace(
  	  regexp_replace (cast (_topic_text as varchar), _link, '', 1, null),
	  _link2,
	  '',
	  1,
	  null);
}
;


create procedure WV.WIKI.FIX_PERMISSIONS ()
{
  declare exit handler for sqlstate '*' {
    rollback work;
    return;
  };
  for select RES_ID as id, RES_PERMS as old_perms, RES_FULL_PATH from WV.WIKI.TOPIC, WS.WS.SYS_DAV_RES where RES_ID = ResId and RES_PERMS like '%N'
  do
   {
     --dbg_obj_princ ('update ', RES_FULL_PATH);
     set triggers off;
     update WS.WS.SYS_DAV_RES set RES_PERMS = replace (old_perms, 'N','R') where RES_ID = id;
     set triggers on;
   }
}
;


-- checks, is content differs from content of existing topic
create function WV.WIKI.DIFFS (in _cluster varchar, in file_name varchar, inout content varchar)
{
  declare exit handler for sqlstate 'DF001' {
    return 1;
  };
  if (not isstring (content))
    return 1;
  if (file_name not like '%.txt')
    return 1;
  declare _topic WV.WIKI.TOPICINFO;
  _topic := WV.WIKI.TOPICINFO();
  _topic.ti_cluster_name := _cluster;
  _topic.ti_fill_cluster_by_name ();
  _topic.ti_local_name := subseq (file_name, 0,length (file_name) - 4);
  _topic.ti_find_id_by_local_name ();
  if (_topic.ti_id = 0)
    return 1;
  _topic.ti_find_metadata_by_id();
  declare _text varchar;
  _text :=  WV.WIKI.DELETE_SYSINFO_FOR (cast (_topic.ti_text as varchar), NULL);
  while (_text[0] and 
  	(_text[length(_text)-1] = 10 or _text[length(_text) -1] = 32))
    _text := subseq (_text, 0, length(_text)-1);
  while (content[0] and 
  	(content[length(content)-1] = 10 or content[length(content)-1] = 32))
    content := subseq (content, 0, length(content)-1);
  if (_text <> content)
    return diff(_text, content);
  return 0;
}
;

-- import procedure 
-- needs existing cluster, and DAV repository where to get the stuff (can be HostFs DET)

create procedure WV.WIKI.IMPORT (in cluster varchar, 
	in source_path varchar, 
	in attachments_path varchar, 
	in auth varchar, 
	in pwd varchar,
	-- when non zero makes checkpoint after importing 
	-- checkpoint_cnt topics
	in checkpoint_cnt int:=1000)	 
{
  declare cluster_path varchar;
  declare cluster_col_id, cluster_id int;
  declare rc int;
  declare checkpoint_idx int;

  cluster_col_id := (select ColId from WV.WIKI.CLUSTERS where ClusterName = cluster);
  if (cluster_col_id is null)
    signal ('WV004', 'Cluster ' || cluster || ' does not exist');
  cluster_id := (select ClusterId from WV.WIKI.CLUSTERS where ClusterName = cluster);
  cluster_path := DB.DBA.DAV_SEARCH_PATH (cluster_col_id, 'C');
  delete from WV.WIKI.LINK 
    where DestId is null or DestId = 0
    and exists (select 1 from WV.WIKI.TOPIC where ClusterId = cluster_id and TopicId = OrigId);

  rc := DAV_AUTHENTICATE (cluster_col_id, 'C', '_1_', auth, pwd);
  if (rc < 0)
    signal ('WV003', auth || ' has not enough credentials to write in ' || cluster_path);

  declare dir_list any;
  declare auth_uid integer;
  auth_uid := DAV_CHECK_AUTH (auth, pwd, 0);
  dir_list := file_dirlist (source_path, 1);
  if (dir_list is null)
    signal ('WV006', 'Can not get directory listing from ' || source_path);
  if(checkpoint_cnt > 0)
    checkpoint_idx := 1;

  declare owner int;
  owner := (select U_ID from DB.DBA.SYS_USERS where U_NAME = auth);

  declare update_list any;
  vectorbld_init (update_list);

  connection_set ('oWiki import', 1);
  foreach (any file_spec in dir_list) do
    {
      declare content, type varchar;
      if (file_spec like '%.txt')
        {
	  content := file_to_string (source_path || file_spec);
	  rc := 1;
	  if (rc < 0)
	    signal ('WV005', 'Can not get content from ' || file_spec || ' [' || DB.DBA.DAV_PERROR (rc) || ']');
	  --dbg_obj_princ ('got from ' || file_spec[10] || ': ', subseq (content, 0, 40));
	  if (file_spec like 'Category%')
	    content := WV.WIKI.ADD_REFBY_MACRO (content);
	  if (WV.WIKI.DIFFS (cluster, file_spec, content))
	    {
  	      WV.WIKI.GETLOCK (DB.DBA.DAV_SEARCH_PATH (cluster_col_id, 'C') ||  file_spec, auth);
	      WV.WIKI.UPLOADPAGE (cluster_col_id, file_spec, content, owner, 0, auth);
--              DB.DBA.DAV_CHECKIN_INT (DB.DBA.DAV_SEARCH_PATH (cluster_col_id, 'C') ||  file_spec, null, null, 0);
	      commit work;
	      result (file_spec);
	      vectorbld_acc (update_list, file_spec);
	      -- result (file_spec[10]);
	      --dbg_obj_princ ('import: ', file_spec[10]);
	      if(checkpoint_cnt > 0)
	        {
		  if (checkpoint_idx < checkpoint_cnt)
		    checkpoint_idx := checkpoint_idx + 1;
		  else
		    {
		      checkpoint_idx := 1;
		      exec ('checkpoint');
		    }
		}
	     }
	 }
     }
  vectorbld_final (update_list);
  connection_set ('oWiki import', NULL);
  WV.WIKI.POSTPROCESS_LINKS (cluster_id);

  declare uid int;
  uid := (select U_ID from DB.DBA.SYS_USERS where U_NAME = auth);

  if(checkpoint_cnt > 0)
    checkpoint_idx := 1;

  whenever sqlstate '40*' default;
  foreach (any file_spec in update_list) do
    {
      if (file_spec like '%.txt')
	{
	  declare _topic WV.WIKI.TOPICINFO;
	  _topic := WV.WIKI.TOPICINFO ();
	  _topic.ti_cluster_name := cluster;
	  _topic.ti_fill_cluster_by_name ();
	  _topic.ti_local_name := subseq (file_spec, 0, length (file_spec) - 4);
	  _topic.ti_find_id_by_local_name();
	  if (_topic.ti_id <> 0)
	    {
	      _topic.ti_find_metadata_by_id ();
      -- render page for set parents etc...      
--	dbg_obj_print ('CHECK TPC ', _topic.ti_local_name);
	      WV.WIKI.CHECK_TOPIC (_topic, auth, pwd);
	       commit work;
	    }
	}
     }


  if (attachments_path is not null)
    dir_list := file_dirlist (attachments_path, 0);
  else
    dir_list := vector();


  whenever sqlstate '40*' goto fix_links;
  foreach (any file_spec in dir_list) do
    {
	  declare attachment_list any;
	  --dbg_obj_princ ('dir list');
      attachment_list := file_dirlist (attachments_path || file_spec, 1);
	  --dbg_obj_princ (' = attachment_list');
	  if (attachment_list is not null)
	    {
  	      declare _topic WV.WIKI.TOPICINFO;
	      _topic := WV.WIKI.TOPICINFO();
	      _topic.ti_cluster_name := cluster;
	      _topic.ti_fill_cluster_by_name();
	  _topic.ti_local_name := file_spec;
	      _topic.ti_find_id_by_local_name();
	      if (_topic.ti_id is not null 
		  and _topic.ti_id > 0)
		{
		  _topic.ti_find_metadata_by_id();
	--dbg_obj_princ (_topic);
		  foreach (any att_spec in attachment_list) do
		    {
		      declare res_id int;
		  declare att_type varchar;
		  declare att_content, att_content2 any;
--		  dbg_obj_princ ('get ', attachments_path || file_spec || '/' || att_spec);
	 	  att_content := file_to_string_output (attachments_path || file_spec || '/' || att_spec); 
--		  att_content := string_output_string (att_content);
		  att_type := DB.DBA.DAV_GUESS_MIME_TYPE_BY_NAME (att_spec);
		  if (att_type is null)
		    att_type := 'application/octet-stream';
		  if (1)
	 	         {
  			    declare att_id any;
		      att_id := DAV_SEARCH_ID (DAV_SEARCH_PATH (_topic.ti_attach_col_id, 'C') || att_spec, 'R');
			    declare content, _type any;
			    if (_topic.ti_attach_col_id = 0 
			     or (coalesce(DAV_HIDE_ERROR (DAV_PROP_GET_INT(
						  att_id, 'R',
						  'oWiki:md5', null, null, 0)), '') 
				  <> md5 (cast (att_content as varbinary))))
			      {
				  -- if (isblob(content) or isstring (content))
					 --dbg_obj_princ ('>', cast (content as varchar), '\n>',  cast (att_content as varchar));
			  	    --dbg_obj_princ ('attach ', att_spec[10], ' ', att_type);
				    declare _res int;
				    _res := WV.WIKI.ATTACH2 (uid,
					  	  att_spec,
			   		 att_type,
				    	_topic.ti_id,
				    	att_content,
				   	'Automatically upload by IMPORT procedure');
				    commit work;
				    result (att_spec);
				    --dbg_obj_princ ('done');
			  	    if(checkpoint_cnt > 0)
				      {
				        if (checkpoint_idx < checkpoint_cnt)
					    checkpoint_idx := checkpoint_idx + 1;
					else	
					  {
					    checkpoint_idx := 1;
					    exec ('checkpoint');
					  }
				      }
			      }
			   }
		    	}
		    }
	    	}
	}
    
 fix_links:
  return 'done';      
}
;

-- checks for <data ... tags, incorrect PARENT macros in topic text
create procedure WV.WIKI.CHECK_TOPIC (
  in _topic WV.WIKI.TOPICINFO,
  in auth varchar,
  in pwd varchar)
{
  if (_topic.ti_local_name is null)
    return NULL;
  declare exit handler for sqlstate '*' {
    --dbg_obj_princ (__SQL_STATE, __SQL_MESSAGE);
    return NULL;
  }
  ;
  connection_set ('WikiV macro TOPICINFO', NULL);
  connection_set ('WIKI params', vector());
  WV.WIKI.VSPXSLT ( 'VspTopicView.xslt', _topic.ti_get_entity (null,1),
    _topic.ti_xslt_vector(vector ('uid', 0,
		'user', auth, 
    		'baseadjust', '../',
		'rnd', 1,
		'attachments', _topic.ti_report_attachments(),
		'is_hist', 0,
		'revision', 0,
		'sid', '',
		'realm', '',
		-- we do not need pretty printing here
		-- anyway output is going to trash,
		-- we need side effect here
		'donotresolve', 1,
		-- tell macros to this is import procedure
		'import', 1)));
  return 1;
}
;

-- generates new password, login can be used, but it is not necessary
create procedure WV.WIKI.GENERATE_NEW_PASSWORD (in login varchar)
{
  declare idx int;
  declare pwd varchar2;
  pwd := make_string (8);
  for (idx:=0; idx<6; idx:=idx+1)
    {
	  pwd[idx] :=  rnd(25) + 97;
	}
  for (idx:=6; idx<8; idx := idx + 1)
    {
	  pwd[idx] := rnd(10) + 48;
	}
  return pwd;
}
;
	  

-- importing users is allowed only to dba
-- usersinfo is a vector of login name, e-mail, and full name (usually WikiWord)
-- if login name already exists, the full name and e-mail are changed if and only 
-- if [rewrite] is not zero. For new logins passwords are generated.
-- Format of userinfo structure:
-- 1. login name
-- 2. e-mail
-- 3. fullname 
create procedure WV.WIKI.IMPORT_USERS (in usersinfo any, 
	in rewrite int := 0, 
	in clusters any := NULL, 
	in add_to_owners integer:= 0)
{
  declare idx int;
  if (not isarray (usersinfo))
    signal ('WV200', 'Userinfo array is expected to be array of array of three elements');
  for (idx:=0; idx<length(usersinfo); idx:=idx+1)
    {
      declare login, e_mail, fullname varchar;
      declare secquestion, secanswer varchar;
      if (not isarray(usersinfo[idx]) or
	      not length(usersinfo[idx]) > 0)
	signal ('WV200', 'Userinfo array is expected to be array of array of three elements');
      login := trim (usersinfo[idx][0]);
      e_mail := usersinfo[idx][1];
      fullname := usersinfo[idx][2];
      secquestion := usersinfo[idx][3];
      secanswer := usersinfo[idx][4];

      declare uid int;
      uid := (select U_ID from SYS_USERS where U_NAME = login);
      if (uid is null)
        {
	  uid := USER_CREATE (login, WV.WIKI.GENERATE_NEW_PASSWORD (login),
		     vector ('E-MAIL', e_mail,
 'FULL_NAME', fullname,
					 'HOME', '/DAV/home/' || login || '/',
					 'DAV_ENABLE' , 1,
					 'SQL_ENABLE', 0));
	  DB.DBA.USER_GRANT_ROLE (login, 'WikiUser', 0);					 
	  -- create new wiki user
	  WV.WIKI.CREATEUSER (login, fullname, 'WikiUser', '', 1);		  
	  USER_SET_OPTION (login, 'SEC_QUESTION', secquestion);
	  USER_SET_OPTION (login, 'SEC_ANSWER', secanswer);
	}
      else if (rewrite)
	{
	  -- update e-mail and full names
	  update SYS_USERS 
	  	set U_E_MAIL = e_mail,
		    U_FULL_NAME = fullname
		where
		    U_ID = uid;
	  if (exists (select * from WV.WIKI.USERS where UserId = uid))
	    {
	      update WV.WIKI.USERS
	  	set UserName = fullname
		where 
		  UserId = uid;
  	    }
	  else
	    {
	      -- create new wiki user
	      WV.WIKI.CREATEUSER (login, fullname, 'WikiUser', '', 1);		  
	    }
	  USER_SET_OPTION (login, 'SEC_QUESTION', secquestion);
	  USER_SET_OPTION (login, 'SEC_ANSWER', secanswer);
	 }
      foreach (varchar cl in clusters) do
        {
	  declare membership_type int;
	  if (add_to_owners)
	    membership_type := 1;
	  else
	    membership_type := 2;
	  if (exists (select * from WV.WIKI.CLUSTERS where ClusterName = cl))
	    {
	      insert into WA_MEMBER (WAM_USER, WAM_INST, WAM_MEMBER_TYPE, WAM_STATUS)
	         values (uid, cl, membership_type, 1);
	      commit work;
	      -- needed to launch trigger
	      update WA_MEMBER set WAM_STATUS = 1
	        where WAM_USER = uid
		  and WAM_INST = cl
		  and WAM_MEMBER_TYPE = membership_type;
 	    }
	}
	 
    }
}
;
			
create procedure WV.WIKI.ADD_TO_READERS (in _cluster_name varchar, in _login varchar)
{
  WV.WIKI.GRANT_CLUSTER_ROLE (_cluster_name || 'Readers', _login);
}
;

create procedure WV.WIKI.ADD_TO_WRITERS (in _cluster_name varchar, in _login varchar)
{
  WV.WIKI.GRANT_CLUSTER_ROLE (_cluster_name || 'Writers', _login);
}
;

create procedure WV.WIKI.GRANT_CLUSTER_ROLE (in _cluster_role varchar, in _login varchar)
{
  if (not exists (select 1 from  SYS_ROLE_GRANTS, SYS_USERS g, SYS_USERS l
	where g.U_NAME = _cluster_role 
	      and l.U_NAME = _login
	      and gi_super = l.U_ID
	      and gi_grant = g.u_id))
    DB.DBA.USER_GRANT_ROLE (_login, _cluster_role);
}
;
	

create procedure WV.WIKI.GET_MAINTOPIC (in _cluster_name varchar)
{
  declare _topic WV.WIKI.TOPICINFO;

  _topic := WV.WIKI.TOPICINFO ();
  _topic.ti_cluster_name := _cluster_name;
  _topic.ti_fill_cluster_by_name();
  _topic.ti_local_name := WV.WIKI.CLUSTERPARAM (_cluster_name, 'index-page', 'WelcomeVisitors');
  _topic.ti_find_id_by_local_name();
  _topic.ti_find_metadata_by_id ();

  if (_topic.ti_id = 0)
    WV.WIKI.APPSIGNAL (11001, 'Main index page does not exist. Please ask administrator to create new one',
      vector () );
  return _topic.ti_id;
}
;
			  
-- it is nice to see REFBY macro in Category pages			  
create procedure WV.WIKI.ADD_REFBY_MACRO (in _text varchar)
{
  return WV.WIKI.ADD_MACRO (_text, '%REFBY%');
}
;
-- adds macro to topic if needed 			  
create procedure WV.WIKI.ADD_MACRO (in _text varchar, in _macro_call varchar)
{
  declare exit handler for sqlstate '*' {
    return _text;
  }
  ;
  if (not isstring (_text))
    return _text;
  -- check, is there the macro already
  declare _macro_name varchar;
  _macro_name := regexp_substr ('%(\\w*)', _macro_call, 1);
  if (length (_macro_name) = 0)
    return _text;    
  if (xpath_eval ('//*/processing-instruction() [name() = "' || _macro_name || '"]', 
  	xtree_doc ( "WikiV lexer" (_text || '\r\n', 'Main', 'DoesntMatter', 'wiki', null), 2)) is not null)
    return _text;
  return _text || '\n' || _macro_call || '\n';
}
;


-- adds macro to topic
-- if topic name contains '*' then adds to any topic which matches this pattern
-- example:
-- WV.WIKI.ADD_MACRO_TO_TOPIC ('Main', 'Category%', '%REFBY%', 'dav', 'dav');
create procedure WV.WIKI.ADD_MACRO_TO_TOPICS (
  in _cluster varchar,
  in _topics varchar,
  in _macro_call varchar,
  in auth varchar,
  in pwd varchar)
{
  
  if (strchr (_topics, '%') is not null)
    {
      for select LocalName
        from WV.WIKI.TOPIC natural join WV.WIKI.CLUSTERS
	where ClusterName = _cluster
	  and LocalName like _topics
        do {
	  WV.WIKI.ADD_MACRO_TO_TOPICS (_cluster, LocalName, _macro_call, auth, pwd);
	}
    }
  declare _topic WV.WIKI.TOPICINFO;
  _topic := WV.WIKI.TOPICINFO ();
  _topic.ti_cluster_name := _cluster;
  _topic.ti_fill_cluster_by_name();
  _topic.ti_local_name := _topics;
  _topic.ti_find_id_by_local_name();
  if (_topic.ti_id = 0)
    return;
  declare rc,owner int;
  
  rc := DAV_AUTHENTICATE (_topic.ti_res_id, 'R', '_1_', auth, pwd);
  if (rc < 0)
    signal ('WV003', auth || ' has not enough credentials to write in ' || _topic.ti_local_name);
  owner := (select U_ID from DB.DBA.SYS_USERS where U_NAME = auth);
  _topic.ti_find_metadata_by_id ();
  declare _new_content varchar;
  _new_content := WV.WIKI.ADD_MACRO (_topic.ti_text, _macro_call);
  WV.WIKI.UPLOADPAGE (_topic.ti_col_id, _topics || '.txt', _new_content, owner, 0, auth);
--  DB.DBA.DAV_CHECKIN_INT (DB.DBA.DAV_SEARCH_PATH (_topic.ti_col_id, 'C') ||  _topics || '.txt', null, null, 0);
}
;

create function WV.WIKI.GETMAINTOPIC_NAME (in _cluster varchar)
{
  return WV.WIKI.CLUSTERPARAM (_cluster, 'index-page', 'WelcomeVisitors');
}
;

grant execute on WV.WIKI.GETMAINTOPIC_NAME to public
;

xpf_extension ('http://www.openlinksw.com/Virtuoso/WikiV/:GetMainTopicName', 'WV.WIKI.GETMAINTOPIC_NAME')
;


create procedure WV.WIKI.CHANGE_WORD_IN_CLUSTER (in _cluster varchar,
  in _word varchar,
  in _new_word varchar,
  in _auth varchar)
{
  declare _topic WV.WIKI.TOPICINFO;
  _topic := WV.WIKI.TOPICINFO();
  _topic.ti_cluster_name := _cluster;
  _topic.ti_fill_cluster_by_name();
  _topic.ti_local_name := _word;
  _topic.ti_find_id_by_local_name();
  if (_topic.ti_id = 0)
    return 0;
  _topic.ti_find_metadata_by_id();
  --dbg_obj_princ ('next: ', _topic.ti_local_name, _auth);
  

  declare cnt int;
  cnt := 0;
  for select OrigId from WV.WIKI.LINK
  	where DestId = _topic.ti_id
  do { 
    --dbg_obj_princ ('next: ', OrigId);
    declare _r_topic WV.WIKI.TOPICINFO;
    declare _owner int;
    _r_topic := WV.WIKI.TOPICINFO();
    _r_topic.ti_id := OrigId;
    _r_topic.ti_find_metadata_by_id();
    _owner := (select RES_OWNER from WS.WS.SYS_DAV_RES where RES_ID = _r_topic.ti_res_id);
    declare _new_res_content varchar;
    _new_res_content := WV.WIKI.CHANGE_LOCALNAME (_r_topic, _topic, _new_word);
    if (_new_res_content is not null)
    {
      --dbg_obj_princ ('updating ', _r_topic.ti_local_name);
      WV.WIKI.UPLOADPAGE (_r_topic.ti_col_id, _r_topic.ti_local_name || '.txt', _new_res_content,
      	_owner, 0, _auth);
--      DB.DBA.DAV_CHECKIN_INT (_r_topic.ti_full_path(), null, null, 0);
      cnt := cnt + 1;
    }
  }
  return cnt;
}
;

create function WV.WIKI.CHANGE_LOCALNAME (
  in _r_topic WV.WIKI.TOPICINFO,
  in _orig_topic WV.WIKI.TOPICINFO,
  in _new_word varchar)
{
  --dbg_obj_princ ('WV.WIKI.CHANGE_LOCALNAME ', _r_topic.ti_local_name, _orig_topic.ti_local_name, _new_word);
  declare _new_content varchar;
  declare _content varchar;
  
  _content := cast (_r_topic.ti_text as varchar);
  _new_word := WV.WIKI.QUALIFY_WORD (_orig_topic.ti_cluster_name, _new_word);

  -- topic can contain non qualified word 
  -- if topics belong to one cluster
  if (_r_topic.ti_cluster_name = _orig_topic.ti_cluster_name)
    {
      _content := WV.WIKI.REPLACE_WORD (_content, 
      	_orig_topic.ti_local_name,
	_new_word);
    }
  -- and replace fully qualified words 
  _content := WV.WIKI.REPLACE_WORD (_content,
    _orig_topic.ti_cluster_name || '.' || _orig_topic.ti_local_name,
    _new_word);
  if (_content <> cast (_r_topic.ti_text as varchar))
    return _content;
  return NULL;
}
;

create function WV.WIKI.QUALIFY_WORD (
  in _def_cluster varchar,
  in _name varchar)
{
  if (strchr(_name, '.') is null)
    return _def_cluster || '.' || _name;
  else
    return _name;
}
;

create function WV.WIKI.REPLACE_WORD (
  in _text varchar,
  in _from varchar,
  in _to varchar)
{
  declare _content, _pattern varchar;
  declare _encoded_from varchar;
  _encoded_from := replace (_from, '.', '\\.');
  _pattern := '^(' || _encoded_from || ')[^0-9A-Za-z]|[^A-Za-z0-9\\.](' || _encoded_from || ')[^0-9A-Za-z]';
  --dbg_obj_print (_pattern);

  declare ss any;
  ss := string_output ();

  declare idx int;
  idx := 0;
  while (idx < length (_text))
    {
      declare regs any;
      regs := regexp_parse (_pattern, _text, idx);
      --dbg_obj_print (regs);
      if (regs is not null)
        {	  
	  http (subseq (_text, idx, regs[0]), ss);
	  idx := regs[1];
	  declare _matched_str varchar;
	  _matched_str := subseq (_text, regs[0], regs[1]);
	  http (replace (_matched_str, _from, _to), ss);	  
	}
      else
        {
	  http (subseq (_text, idx), ss);
	  idx := length (_text);
	}
    }
  return string_output_string (ss);
}
;


create function WV.WIKI.CATEGORY_TO_TAG (in _cat varchar)
{
  if (_cat like 'Category%')
    return lcase (subseq (_cat, 8));
  return NULL;
}
;

-- looks for category in topics first
-- if failed, creates new category
create function WV.WIKI.TAG_TO_CATEGORY (in _tag varchar, in _cluster_id int)
{
  declare _cat_name varchar;
  _cat_name := lcase ('Category' || _tag);
  for select LocalName from WV.WIKI.TOPIC
  	where
	  ClusterId = _cluster_id
	  and LocalName like 'Category%'
	  and lcase (LocalName) = _cat_name
  do {
    return LocalName;
  }
  return 'Category' || WV.WIKI.CAPITALIZE (_tag);
}
;


create function WV.WIKI.CAPITALIZE (in _tag varchar)
{  
  return ucase (subseq (_tag, 0, 1)) || lcase (subseq (_tag, 1));
}
;

create function WV.WIKI.AUTH_BY_LDAP (in _cluster any, in _user varchar, in _pwd varchar)
{
   --dbg_obj_princ ('AUTH_BY_LDAP ', _cluster, ' ', _user, ' ', _pwd);
  declare _address, _base, _bind, _uid_field, _port  varchar;  
  _address := WV.WIKI.CLUSTERPARAM (_cluster, 'ldap_address', '');
  _base := WV.WIKI.CLUSTERPARAM (_cluster, 'ldap_base', '');
  _bind := WV.WIKI.CLUSTERPARAM (_cluster, 'ldap_bind', '');
  _uid_field := WV.WIKI.CLUSTERPARAM (_cluster, 'ldap_uid', '');
  _port := WV.WIKI.CLUSTERPARAM (_cluster, 'ldap_port', '389');

  whenever sqlstate '28000' goto auth_validation_fail;
  connection_set ('LDAP_VERSION', WV.WIKI.CLUSTERPARAM (_cluster, 'ldap_version', 2));
  LDAP_SEARCH('ldap://' || _address || ':' || _port,
  	0, 
	_base, 
	sprintf ('(%s=%s)', _uid_field, _user),
	sprintf('%s=%s, %s', _uid_field, _user, _bind),
	_pwd);
   --dbg_obj_print ('sucess');
  return 1;
auth_validation_fail:
  return 0;
}
;
	 
create procedure WV.WIKI.INC_COMMITCOUNTER (in _uid int)
{
  declare _cnt int;
  whenever not found goto ins;
  declare cr cursor for select Cnt from WV.WIKI.COMMITCOUNTER where AuthorId = _uid for update;
  open cr;
  fetch cr into _cnt;
  update WV.WIKI.COMMITCOUNTER set Cnt = _cnt + 1
  	where current of cr;
  return;
ins:
  insert into WV.WIKI.COMMITCOUNTER (AuthorId, Cnt)
  	values (_uid, 1);
}
;
      
create procedure WV.WIKI.SYSINFO_PRED ()
{
  return 'DE3A857A5FFB11DA923AF0924C194AED';
}
;

create procedure WV.WIKI.XHTML_PREFIX ()
{
  return 'D001AF62737B11DA9C6DDEE3AE8897E7';
}
;


create function WV.WIKI.ENCODE_FT (in _term varchar, in _other_text varchar, in encode_ft int := 1)
{
  if (not encode_ft)
    return _term || '\n' ||_other_text;
  else if (_other_text is not null)
    return '"' || _term || '" AND ' || _other_text;
  else 
    return '"' || _term || '"';
}
;

create procedure WV.WIKI.ADD_SYSINFO (in _text varchar, in _predicate varchar, in _value varchar, in encode_ft int := 0)
{
  declare _pred varchar;
  _pred := WV.WIKI.SYSINFO_PRED() || ' ' || _predicate || ' ' || _value;
  if (strcasestr (_text, _pred) is null)
    return WV.WIKI.ENCODE_FT (_pred, _text, encode_ft);
  else
    return _text;
}
;

create procedure WV.WIKI.ADD_SYSINFO_VECT (in _text varchar,
	in _info any, in encode_ft int := 0)
{
  --dbg_obj_princ ('WV.WIKI.ADD_SYSINFO_VECT ', _text, ' ', _info);
  declare i int;
  declare _sysinfo_text varchar;
  _sysinfo_text := null;
  for (i:=0; i<(length (_info)/2) ;i:=i+1)
    _sysinfo_text :=  WV.WIKI.ADD_SYSINFO (_sysinfo_text, _info[2*i], _info[2*i+1], encode_ft);
  if (_text is not null and encode_ft)
    return _sysinfo_text || ' AND ' || _text;
  return _sysinfo_text || _text;
}
;

create function WV.WIKI.DELETE_SYSINFO_FOR (in _text varchar, in _pred varchar := NULL)
{
  declare _lines any;
  declare _res any;
  declare _prefix varchar;
  if (not isstring(_text))
    _text := cast (_text as varchar);
  if (_pred is not null)
    _prefix := WV.WIKI.SYSINFO_PRED() || ' ' || _pred || ' %';
  else
    _prefix := WV.WIKI.SYSINFO_PRED() || ' %';
  
  _lines := split_and_decode (_text, 0, '\0\0\n');
  _res := string_output();
  foreach (varchar l0 in _lines) do
    { 
      if (l0  not like _prefix)
        {
	  http (l0, _res);
	  http ('\n', _res);
	}
    }
  return string_output_string (_res);
}
;

create trigger "Wiki_Tagging" after insert on WS.WS.SYS_DAV_TAG referencing new as N
{  
  --dbg_obj_princ ('Wiki_Tagging trigger');
  if (exists (select 3 from WV.WIKI.TOPIC where ResId = N.DT_RES_ID))
    WV.WIKI.UPDATE_TAG_SYSINFO (N.DT_RES_ID, N.DT_TAGS);
}
;

create trigger "Wiki_TaggingUpdate" after update on WS.WS.SYS_DAV_TAG referencing new as N
{
  --dbg_obj_princ ('Wiki_TaggingUpdate trigger');
  if (exists (select 1 from WV.WIKI.TOPIC where ResId = N.DT_RES_ID))
    WV.WIKI.UPDATE_TAG_SYSINFO (N.DT_RES_ID, N.DT_TAGS);
}
;

create procedure WV.WIKI.UPDATE_TAG_SYSINFO (in _res_id int, in _tags varchar)
{
  return;

  --dbg_obj_princ ('WV.WIKI.UPDATE_TAG_SYSINFO: ', _res_id, _tags);
      declare _col_id int;
      declare _topic_file_name, _full_path varchar;
      declare _content varchar;
      declare _owner int;
      declare _auth varchar;
      declare _nobody_uid int;
      declare _all_tags any;

      _all_tags := (select vector_agg (WV.WIKI.ZIP ('tag', split_and_decode (DT_TAGS, 0, '\0\0,')))
      	from WS.WS.SYS_DAV_TAG where DT_RES_ID = _res_id);
      
      select RES_CONTENT, RES_OWNER, RES_COL, RES_NAME, RES_FULL_PATH
      	into _content, _owner, _col_id, _topic_file_name, _full_path from WS.WS.SYS_DAV_RES
	where RES_ID = _res_id;	
      _content := WV.WIKI.DELETE_SYSINFO_FOR (cast (_content as varchar), 'tag');	
            
      _auth := coalesce (connection_get ('vspx_user'), 'WikiGuest');     
       --dbg_obj_princ (_owner, _auth, _col_id, _topic_file_name, WV.WIKI.ADD_SYSINFO_VECT (_content, WV.WIKI.FLATTEN (_all_tags)));
      declare res int;
      connection_set ('oWiki trigger', 1);

      declare _owner_login varchar;
      _owner_login := (select U_NAME from DB.DBA.SYS_USERS where U_ID = _owner);
      res := WV.WIKI.GETLOCK (_full_path, _owner_login);
      --dbg_obj_princ ('lock: ', DAV_PERROR (res));
      res := WV.WIKI.UPLOADPAGE (_col_id, _topic_file_name, 
      	WV.WIKI.ADD_SYSINFO_VECT (_content, 
		WV.WIKI.FLATTEN (_all_tags)),
      	 _owner_login, 0, _auth);
--      DAV_CHECKIN_INT (_full_path, null, null, 0);
      WV.WIKI.RELEASELOCK (_full_path, _owner_login);
      connection_set ('oWiki trigger', NULL);
      --dbg_obj_princ ('upload: ', DAV_PERROR (res));
}
;


create procedure WV.WIKI.SET_AUTOVERSION()
{
  for select ColId from WV.WIKI.CLUSTERS, WS.WS.SYS_DAV_COL
    where COL_ID = ColId and COL_AUTO_VERSIONING is not null and COL_AUTO_VERSIONING <> 'A' do
    {
      declare _auth, _pwd varchar;
      WV.WIKI.GETDAVAUTH (_auth, _pwd);
      DAV_SET_VERSIONING_CONTROL (
        DAV_SEARCH_PATH(ColId, 'C'), NULL, 'A', _auth, _pwd);
    }
}
;


create procedure WV.WIKI.STALE (in _path varchar)
{
  xslt_stale (_path);
}
;

create procedure WV.WIKI.STALE_ALL_XSLTS()
{
  declare _prefix varchar;
  _prefix := 'virt://WS.WS.SYS_DAV_RES.RES_FULL_PATH.RES_CONTENT:/DAV/VAD/wiki/Root/';
  -- Skins
  for select COL_NAME from WS.WS.SYS_DAV_COL 
    where COL_PARENT = DB.DBA.DAV_SEARCH_ID ('/DAV/VAD/wiki/Root/Skins/', 'C') do
     {
       WV.WIKI.STALE (_prefix || 'Skins/' || COL_NAME || '/PostProcess.xslt');
        --dbg_obj_princ (_prefix || 'Skins/' || COL_NAME || '/PostProcess.xslt');
     }
  for select RES_NAME from WS.WS.SYS_DAV_RES 
    where RES_COL = DB.DBA.DAV_SEARCH_ID ('/DAV/VAD/wiki/Root/', 'C') 
    and RES_NAME like '%.xsl%' do
     {
       WV.WIKI.STALE (_prefix || RES_NAME);
        --dbg_obj_princ (_prefix || RES_NAME);
     }
}
;


create trigger "Wiki_TopicDelete" before delete on WV.WIKI.TOPIC referencing old as O
{
  delete from WV.WIKI.COMMENT where C_TOPIC_ID = O.TopicId;
}
;

      
create procedure WV.WIKI.ADD_USER (in user_name varchar, in cluster_name varchar)
{
  declare membership_type, uid int;
  membership_type := 1;
  uid := (select U_ID from DB.DBA.SYS_USERS where U_NAME = user_name);
  if (exists (select * from WV.WIKI.CLUSTERS where ClusterName = cluster_name))
    {
      insert into WA_MEMBER (WAM_USER, WAM_INST, WAM_MEMBER_TYPE, WAM_STATUS)
	         values (uid, cluster_name, membership_type, 1);
      commit work;
      -- needed to launch trigger
      update WA_MEMBER set WAM_STATUS = 1
        where WAM_USER = uid
	  and WAM_INST = cluster_name
	  and WAM_MEMBER_TYPE = membership_type;
    }
}
;

create procedure WV.WIKI.DROP_ALL_MEMBERS ()
{
  for select WAI_NAME from DB.DBA.WA_INSTANCE where WAI_TYPE_NAME = 'oWiki' do {
    delete from DB.DBA.WA_MEMBER where WAM_INST = WAI_NAME;
  }
}
;
   
create procedure WV.WIKI.USER_WIKI_NAME(in user_name varchar)
{
  return cast (WV.WIKI.CONVERTTITLETOWIKIWORD(user_name) as varchar);
}
;

create procedure WV.WIKI.USER_WIKI_NAME_2(in user_id int)
{
  return cast (WV.WIKI.CONVERTTITLETOWIKIWORD( (select USERNAME from WV.WIKI.USERS where USERID = user_id) ) as varchar);
}
;

create procedure WV.WIKI.USER_WIKI_NAME_X (in _uid any)
{
  if (isstring (_uid))
    return 'Main.' || WV.WIKI.USER_WIKI_NAME ((select USERNAME from WV.WIKI.USERS, DB.DBA.SYS_USERS where U_ID = USERID and U_NAME =_uid));
  else if (isinteger (_uid))
    return 'Main.' || WV.WIKI.USER_WIKI_NAME_2 (_uid);
  else
    return 'Main.Unknown';
}
;

create procedure WV.WIKI.TEMPLATE_TOPIC(in template_name varchar)
{
  declare _topic_name varchar;
  _topic_name := WV.WIKI.CONVERTTITLETOWIKIWORD ('Template ' || template_name);
  declare _topic WV.WIKI.TOPICINFO;
  _topic := WV.WIKI.TOPICINFO ();
  _topic.ti_default_cluster := 'Main';
  _topic.ti_raw_title := _topic_name;
  _topic.ti_find_id_by_raw_title ();
  if (_topic.ti_id <> 0)
    _topic.ti_find_metadata_by_id ();

  return _topic;
}
;

create procedure WV.WIKI.REPLACE_BULK (in _text varchar,
	in _repls any)
{
  declare idx int;
  for (idx:=0; idx<length(_repls); idx:=idx+2)
    {
      _text := replace (_text, _repls[idx], coalesce (_repls[idx+1], ''));
    }
  return _text;
}
;

create procedure WV.WIKI.CREATE_HOME_PAGE_TOPIC (inout _template WV.WIKI.TOPICINFO,
  	in home_page varchar,
	in user_id int)
{
  for select U_NAME,USERNAME, U_E_MAIL, coalesce (U_FULL_NAME, U_NAME) as _full_name from DB.DBA.SYS_USERS, WV.WIKI.USERS 
	where U_ID = user_id 
	and U_ID = USERID do
  {
    declare _text varchar;
    _text := WV.WIKI.REPLACE_BULK (cast (_template.ti_text as varchar), 
	vector ('{USER}', USERNAME, '{FULLNAME}', _full_name, '{EMAIL}', U_E_MAIL));
    WV.WIKI.UPLOADPAGE (_template.ti_col_id, home_page, _text, U_NAME);
    return;
  }
}
;

create procedure WV.WIKI.CREATE_USER_PAGE(in user_id varchar, in user_name varchar)
{
  declare home_page varchar;
  home_page := WV.WIKI.USER_WIKI_NAME (user_name);
  if (exists (select 1 from WV.WIKI.TOPIC natural join WV.WIKI.CLUSTERS
	where LocalName = home_page and ClusterName = 'Main'))
     return ;
  declare _template WV.WIKI.TOPICINFO;
  _template := WV.WIKI.TEMPLATE_TOPIC ('user');
  if (_template.ti_id = 0)
        WV.WIKI.APPSIGNAL (11001, 'Can not get template for creating home page', vector());
  _template.ti_text := cast (_template.ti_text as varchar);
  
  declare _text varchar;
  _text := WV.WIKI.CREATE_HOME_PAGE_TOPIC (_template, home_page, user_id);
}
;

create procedure WV.WIKI.CREATE_ALL_USERS_PAGES ()
{
  for select USERID, USERNAME from WV.WIKI.USERS do
    {
      WV.WIKI.CREATE_USER_PAGE(USERID, USERNAME);
    }
}
;


create trigger WIKI_USERS_I after insert on WV.WIKI.USERS order 100 referencing new as N
{
   WV.WIKI.CREATE_USER_PAGE (N.USERID, N.USERNAME);
}
;

create trigger WIKI_USERS_U after update on WV.WIKI.USERS order 100 referencing old as O, new as N
{
  if (N.USERNAME = O.USERNAME)
    return;
  declare _topic WV.WIKI.TOPICINFO;
  _topic := WV.WIKI.TOPICINFO();
  _topic.ti_raw_title := WV.WIKI.USER_WIKI_NAME(O.USERNAME);
  _topic.ti_default_cluster := 'Main';
  _topic.ti_find_id_by_raw_title();
  if (_topic.ti_id <> 0)
    {
      _topic.ti_fill_cluster_by_id();
      _topic.ti_find_metadata_by_id();
      WV.WIKI.RENAMETOPIC (_topic, N.USERNAME, _topic.ti_cluster_id, WV.WIKI.USER_WIKI_NAME (N.USERNAME));
    }
}
;

create trigger SYS_USERS_WIKI_USERS_U after update on DB.DBA.SYS_USERS order 100 referencing old as O, new as N
{
  if (O.U_FULL_NAME <> N.U_FULL_NAME)
    {
      update WV.WIKI.USERS 
	set USERNAME = WV.WIKI.USER_WIKI_NAME(N.U_FULL_NAME) 
	where USERID = N.U_ID;
    }
}
;

create procedure
WS.WS.META_WIKI_HOOK (inout vtb any, inout r_id any)
{
  --dbg_obj_princ ('WS.WS.META_WIKI_HOOK: ', r_id);
  declare exit handler for sqlstate '*' {
    --dbg_obj_princ (__SQL_STATE, ' ', __SQL_MESSAGE);
    return;
  };
  declare _cluster varchar;
  _cluster := (select ClusterName from WV.WIKI.TOPIC natural join WV.WIKI.CLUSTERS 
	where ResId = r_id);
  if (_cluster is not null)
    {
      -- it is a wiki
      foreach (varchar tag in (select split_and_decode (DT_TAGS, 0, '\0\0,')
       	from WS.WS.SYS_DAV_TAG where DT_RES_ID = r_id)) 
      do 
        {
          vt_batch_feed (vtb, WV.WIKI.SYSINFO_PRED () || ' tag ' || tag, 0);
        }
      vt_batch_feed (vtb, WV.WIKI.SYSINFO_PRED () || ' cluster ', 0);
      vt_batch_feed (vtb, WV.WIKI.SYSINFO_PRED () || ' cluster ' || _cluster, 0);
      --dbg_obj_princ (WV.WIKI.SYSINFO_PRED () || ' cluster ' || _cluster);
    }
}
;

create procedure WV.WIKI.SANITY_CHECK()
{
  set triggers off;
  for select WAI_NAME  as _name from DB.DBA.WA_INSTANCE where WAI_TYPE_NAME = 'oWiki' and not exists (select * from WV.WIKI.CLUSTERS where CLUSTERNAME = WAI_NAME) 
  do 
     {
        delete from DB.DBA.WA_INSTANCE where WAI_NAME = _name and WAI_TYPE_NAME = 'oWiki';
     }
  for select WAM_INST as _name from DB.DBA.WA_MEMBER 
    where WAM_APP_TYPE = 'oWiki' 
    and not exists (select 1 from WV.WIKI.CLUSTERS where CLUSTERNAME = WAM_INST)
  do 
    {
       delete from DB.DBA.WA_MEMBER where WAM_INST = _name and WAM_APP_TYPE = 'oWiki';
    }
  set triggers on;
  declare vtb any;
  for select RES_ID, RES_FULL_PATH from WS.WS.SYS_DAV_RES 
    where contains (RES_CONTENT, '"DE3A857A5FFB11DA923AF0924C194AED cluster "') 
    and not exists (select 1 from WV.WIKI.TOPIC where RESID = RES_ID)
  do {
    vtb := vt_batch ();
    vt_batch_d_id (vtb, RES_ID);
    vt_batch_feed (vtb, '"DE3A857A5FFB11DA923AF0924C194AED cluster "', 1);
    vt_batch_feed (vtb, 'DE3A857A5FFB11DA923AF0924C194AED', 1);
    WS.WS.VT_BATCH_PROCESS_WS_WS_SYS_DAV_RES (vtb);
  }
}
;