\n
By default all URLs (in a playlist) with path component changes are treated as new services. 
This causes channels linked to them to be marked ```{name-not-set}``` 
due to the previously-used URL "no longer existing" in the playlist, they in fact do exist but have changed slightly.

Setting a number here forces tvheadend to ignore frequently-changing 
path components when deciding if a URL is new or not - starting from the end of the URL..

Here are some examples:-

URL in playlist                                                 | Frequently changing path component(s) in URL   | Number of components to ignore | Treated as new if changed?                            
----------------------------------------------------------------|------------------------------------------------|--------------------------------|-----------------------------------------------------
```http://ip.tv/channel/BBC1/id/dHZoZWFkZW5kLm9yZw==```         | ```dHZoZWFkZW5kLm9yZw==```                     | 0                              | Yes, because no components are ignored.
```http://ip.tv/channel/BBC1/id/dHZoZWFkZW5kLm9yZw==```         | ```dHZoZWFkZW5kLm9yZw==```                     | 1                              | No, because we're ignoring the last component ```dHZoZWFkZW5kLm9yZw==```.
```http://ip.tv/channel/BBC4/id/dHZoZWFkZW5kLm9yZw==/1234```    | ```dHZoZWFkZW5kLm9yZw==``` / ```1234```        | 1                              | Yes, but only if the second-from-last component ```dHZoZWFkZW5kLm9yZw==``` changes. We're ignoring the last component ```1234```, so if that changes it won't make any difference.
```http://ip.tv/channel/BBC4/id/dHZoZWFkZW5kLm9yZw==/1234```    | ```dHZoZWFkZW5kLm9yZw==``` / ```1234```        | 2                              | No, because we're ignoring the last two components ```dHZoZWFkZW5kLm9yZw==```/```1234```.
