# Database Schemas

There are several database schemas used for OpenStreetMap data. The
primary one that operates the core servers uses **apidb**. There is a
simplified version called **pgsnapshot**, which is more suited towards
running ones own database. Both of these schemas are the only ones
that can be updated with replication files. A replication file is the
data of the actual change, and available with minutely, hourly, or
daily updates.

Osmosis is commonly used for manipulating and importing data into
these schemas. Although it has been unmaintained for several
years. It’s written in Java, and as it uses temporary files on disk,
an import can consume upwards of a terabyte.

Underpass can import a data file into the *pgsnapshot* schema without
*osmosis*. It can use replication files to update the database. 

## pgsnapshot

The *pgsnapshot* schema is a more compact version of the main
database. It contains geospatial information, and can be updated using
replication files. It can also be easily queried to produce data
extracts in OSM format. By default, it is not well suited to any data
analysis that uses geospatial calculations since the nodes are stored
separately from the ways. Underpass extends this schema by by adding a
geometry column to the ways table.

schema.

&nbsp;

Keyword | Description
--------|------------
actions | The action to do for a change,ie.. Create, delete, or modify
locked | Locked when updating the database
nodes | All the nodes
relation_members | The memebers in a relation
relations | A list of replations
replication_changes | Counters on the changes in this replication file
schema_info | Version number of this schema
spatial_ref_sys | Geospatial data used by Postgis
sql_changes | Used when applying a replication file
state | The stat of the database update
users | A list of user IDs and usernames
way_nodes | A list of node references in a way
ways | A list of ways

## apidb

The *apidb* schema is the one that operates the core OpenStreetMap
servers, and is usually access through a REST API.


Keyword | Description |
--------|------------ |
acls | tbd
active_storage_attachments | tbd
active_storage_blobs | tbd
ar_internal_metadata | tbd
changeset_comments | tbd
changeset_tags | tbd
changesets | tbd
changesets_subscribers | tbd
client_applications | tbd
current_node_tags | tbd
current_nodes | tbd
current_relation_members | tbd
current_relation_tags | tbd
current_relations | tbd
current_way_nodes | tbd
current_way_tags | tbd
current_ways | tbd
delayed_jobs | tbd
diary_comments | tbd
diary_entries | tbd
diary_entry_subscriptions | tbd
friends | tbd
gps_points | tbd
gpx_file_tags | tbd
gpx_files | tbd
issue_comments | tbd
issues | tbd
languages | tbd
messages | tbd
node_tags | tbd
nodes | tbd
note_comments | tbd
notes | tbd
oauth_nonces | tbd
oauth_tokens | tbd
redactions | tbd
relation_members | tbd
relation_tags | tbd
relations | tbd
reports | tbd
schema_migrations | tbd
spatial_ref_sys | tbd
user_blocks | tbd
user_preferences | tbd
user_roles | tbd
user_tokens | tbd
users | tbd
way_nodes | tbd
way_tags | tbd
ways | tbd

&nbsp;
## osm2pgsql

Osm2pgsql is the primary tool for importing OSM files. The *osm2pgsql*
program can import a data file into postgres, but it doesn’t support
updating the data. As it contains geospatial data, it’s a compact and
easily queried schema. It lacks any knowledge of relations, which
isn’t usually a problem unless you want to do deeper analysis of the
data.

&nbsp;

Keyword | Description |
--------|------------ |
planet_osm_line | All of the ways that aren't a polygon
planet_osm_point | All of the nodes
planet_osm_polygon | All of the polygons
planet_osm_roads | Unused currently
spatial_ref_sys | Geospatial data for postgis

&nbsp;
## ogr2ogr

Ogr and GDAL are the basis for many GIS projects, but has minimal OSM
data file support. It can read any of the file formats, and can import
into a database. It does have one big improvement over the osm2pgsql
format, namely better relation support. This schema can be used to
calculate user statistics. 

&nbsp;

Keyword | Description |
--------|------------ |
lines | All of the single lines
multilinestrings | All of the multiple lines
multipolygons | All of the polygons
other_relations | All of the tags
points | All of the nodes
spatial_ref_sys | Geospatial data for postgis