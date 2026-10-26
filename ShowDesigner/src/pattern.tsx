import { HexColorPicker } from "react-colorful";

export enum PatternType {
    SHOW,
    FADE,
    DARK,
    UNDEFINED
}

export class Pattern {
    static PatternType = PatternType;

    patternType: PatternType = PatternType.UNDEFINED;
    duration: number = 0;
    color1: string = "#000000";
    color2: string = "#000000";

    showPicker1: boolean = false;
    showPicker2: boolean = false;

    constructor(patType: PatternType = PatternType.SHOW, patDur: number = 1, col1: string = "#ffffff", col2: string = "#ffffff") {
        this.patternType = patType;
        this.duration = patDur;
        this.color1 = col1;
        this.color2 = col2;
    }

    print() {
        console.log(this.patternType);
        console.log(this.duration);
        console.log(this.color1);
        console.log(this.color2);
    }

    display(index: number, onUpdate?: () => void) {
        switch(this.patternType) {
            case PatternType.SHOW:
                return this.displayShow(index, onUpdate);

            case PatternType.FADE:
                    return this.displayFade(index, onUpdate);

            default:
                console.log("other");
                return null;
        }
    }

    displayShow(index: number, onUpdate?: () => void) {
        return (
            <tr key={index}>
                <td>{index + 1}.</td>
                <td>Show color</td>
                {this.displayColor1(onUpdate)}
                <td>for</td>
                <td>{this.duration}s</td>
            </tr>
        );
    }

    displayFade(index: number, onUpdate?: () => void) {
        return (
            <tr key={index}>
                <td>{index + 1}.</td>
                <td>Fade from color</td>
                {this.displayColor1(onUpdate)}
                <td>to</td>
                {this.displayColor2(onUpdate)}
                <td>over</td>
                <td>{this.duration} seconds</td>
            </tr>
        );
    }

    displayColor1(onUpdate?: () => void) {
        return (
            <td style={{ position: "relative" }}>
                <div 
                    onClick={(e) => {
                        e.stopPropagation();
                        this.showPicker1 = !this.showPicker1;
                        this.showPicker2 = false;
                        if (onUpdate) onUpdate();
                    }}
                    style={{
                        width: "28px",
                        height: "28px",
                        backgroundColor: this.color1,
                        border: "2px solid #fff",
                        boxShadow: "0 0 0 1px #ccc",
                        borderRadius: "4px",
                        cursor: "pointer"
                    }}
                />
                {this.showPicker1 && (
                    <>
                        <div 
                            onClick={(e) => {
                                e.stopPropagation();
                                this.showPicker1 = false;
                                if (onUpdate) onUpdate();
                            }}
                            style={{
                                position: "fixed",
                                top: 0,
                                left: 0,
                                width: "100vw",
                                height: "100vh",
                                zIndex: 998,
                                background: "transparent"
                            }}
                        />
                        {/* Popover Window */}
                        <div style={{ position: "absolute", zIndex: 999, top: "100%", left: 0, background: "#222", padding: "10px", borderRadius: "6px", boxShadow: "0 4px 12px rgba(0,0,0,0.3)" }}>
                            <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
                                <HexColorPicker 
                                    color={this.color1} 
                                    onChange={(newColor) => {
                                        this.color1 = newColor;
                                        if (onUpdate) onUpdate();
                                    }} 
                                />
                                <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                                    <span style={{ fontSize: "12px", color: "#aaa" }}>#</span>
                                    <input 
                                        type="text" 
                                        value={this.color1.replace("#", "")} 
                                        onChange={(e) => {
                                            const sanitized = e.target.value.replace(/[^0-9A-Fa-f]/g, "").slice(0, 6);
                                            this.color1 = "#" + sanitized;
                                            if (onUpdate) onUpdate();
                                        }}
                                        style={{
                                            background: "#111",
                                            border: "1px solid #444",
                                            color: "#fff",
                                            padding: "4px 6px",
                                            borderRadius: "4px",
                                            width: "80px",
                                            fontSize: "12px",
                                            outline: "none"
                                        }}
                                    />
                                </div>
                            </div>
                        </div>
                    </>
                )}
            </td>
        );
    }

    displayColor2(onUpdate?: () => void) {
        return (
            <td style={{ position: "relative" }}>
                <div 
                    onClick={(e) => {
                        e.stopPropagation();
                        this.showPicker2 = !this.showPicker2;
                        this.showPicker1 = false;
                        if (onUpdate) onUpdate();
                    }}
                    style={{
                        width: "28px",
                        height: "28px",
                        backgroundColor: this.color2,
                        border: "2px solid #fff",
                        boxShadow: "0 0 0 1px #ccc",
                        borderRadius: "4px",
                        cursor: "pointer"
                    }}
                />
                {this.showPicker2 && (
                    <>
                        <div 
                            onClick={(e) => {
                                e.stopPropagation();
                                this.showPicker2 = false;
                                if (onUpdate) onUpdate();
                            }}
                            style={{
                                position: "fixed",
                                top: 0,
                                left: 0,
                                width: "100vw",
                                height: "100vh",
                                zIndex: 998,
                                background: "transparent"
                            }}
                        />
                        {/* Popover Window */}
                        <div style={{ position: "absolute", zIndex: 999, top: "100%", left: 0, background: "#222", padding: "10px", borderRadius: "6px", boxShadow: "0 4px 12px rgba(0,0,0,0.3)" }}>
                            <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
                                <HexColorPicker 
                                    color={this.color2} 
                                    onChange={(newColor) => {
                                        this.color2 = newColor;
                                        if (onUpdate) onUpdate();
                                    }} 
                                />
                                <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                                    <span style={{ fontSize: "12px", color: "#aaa" }}>#</span>
                                    <input 
                                        type="text" 
                                        value={this.color2.replace("#", "")} 
                                        onChange={(e) => {
                                            const sanitized = e.target.value.replace(/[^0-9A-Fa-f]/g, "").slice(0, 6);
                                            this.color2 = "#" + sanitized;
                                            if (onUpdate) onUpdate();
                                        }}
                                        style={{
                                            background: "#111",
                                            border: "1px solid #444",
                                            color: "#fff",
                                            padding: "4px 6px",
                                            borderRadius: "4px",
                                            width: "80px",
                                            fontSize: "12px",
                                            outline: "none"
                                        }}
                                    />
                                </div>
                            </div>
                        </div>
                    </>
                )}
            </td>
        );
    }
}