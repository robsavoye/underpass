with building as (SELECT ST_Centroid(geometry) as geometry from raw_poly where tags ? 'building' limit 1) SELECT ST_Y(geometry)::text || ',' || ST_X(geometry)::text from building;