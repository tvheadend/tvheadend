

Use this setting to translate broadcaster-specific, country-specific or other customised genre tags into tags recognised by tvheadend.

Example:

```
192=20
208=16
224=35
```

| Line        | Meaning         |
| ------------- | ------------- |
|192=20|Translate decimal 192 (0xC0 = Australian-specific 'comedy') to decimal 20 (0x14 = ETSI standard 'comedy').|
|208=16|Translate decimal 208 (0xD0 = Australian-specific 'drama') to decimal 16 (0x10 = ETSI standard 'movie/drama (general)').|
|224=35|Translate decimal 224 (0xE0 = Australian-specific 'documentary') to decimal 35 (0x23 = ETSI standard 'documentary').|

See: [Search the ETSI web site for the latest version of the 'ETSI EN 300 468' standard.](https://www.etsi.org/standards#page=1&search=%22ETSI%20EN%20300%20468%22&title=0&etsiNumber=1&content=0&version=1&onApproval=1&published=1&withdrawn=1&historical=1&isCurrent=1&superseded=1&startDate=1988-01-15&endDate=2023-07-09&harmonized=0&keyword=&TB=&stdType=&frequency=&mandate=&collection=&sort=1)

Search for 'content_descriptor' in the standards document.