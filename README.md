ngx_http_geoip_formt_module
===========================

get geoip info from url args

example conf:


        location /geoinfo {
            geoip_format_city /usr/share/GeoIP/GeoLiteCity.dat;
        }


curl localhost/geoinfo?ip=114.114.114.114
