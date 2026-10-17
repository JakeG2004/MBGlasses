# Glasses

Finding and documentation of what was found during the glasses repair.

## Documentation

Connectors are JST-PH 2.0mm.

Wiring pinouts are as follows:

### Battery Harness (2-Pin JST-PH 2.0mm)

| Pin | Wire Color | Signal / Destination |
| :--- | :--- | :--- |
| Pin 1 | Red | Battery (+) |
| Pin 2 | Black | Battery (-) |

```mermaid
graph LR
    subgraph JST_BAT["JST-PH 2.0mm (2-Pin)"]
        P1["Pin 1"]
        P2["Pin 2"]
    end
    subgraph BAT["Battery"]
        BP["Battery (+)"]
        BN["Battery (-)"]
    end
    P1 -->|"Red wire"| BP
    P2 -->|"Black wire"| BN
```

### LED Harness (4-Pin JST-PH 2.0mm)

| Pin | Wire Color | Signal / Destination |
| :--- | :--- | :--- |
| Pin 1 | White | VCC |
| Pin 2 | Red | Red Channel |
| Pin 3 | Green | Green Channel |
| Pin 4 | Blue | Blue Channel |

```mermaid
graph LR
    subgraph JST_LED["JST-PH 2.0mm (4-Pin)"]
        LP1["Pin 1"]
        LP2["Pin 2"]
        LP3["Pin 3"]
        LP4["Pin 4"]
    end
    subgraph LED["LED Module"]
        VCC["VCC"]
        R["Red Channel"]
        G["Green Channel"]
        B["Blue Channel"]
    end
    LP1 -->|"White wire"| VCC
    LP2 -->|"Red wire"| R
    LP3 -->|"Green wire"| G
    LP4 -->|"Blue wire"| B
```

## Findings

Leads from the battery are flipped midway through, due to the pins being flipped
inside the JST connector. 



