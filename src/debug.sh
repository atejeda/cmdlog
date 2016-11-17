curl -XGET -v 'http://localhost:9200/_all/_search' -d '{
    "from" : 0,
    "size" : 1,
    "highlight": {
        "fields": {
          "*": { }
        },
        "require_field_match": false,
        "fragment_size": 2147483647
    },
    "query": {
        "query_string":  {
            "default_field" : "text",
            "query": "Host:gas06 AND text:fullauto AND tags:AOS64",
            "analyze_wildcard": true
        }
    },
    "sort": [
        { 
            "@timestamp": { 
                "order": "desc" 
            }
        }
    ]
}';
echo "";
# http://localhost:9200/_all/_search?pretty=true?q=DV20