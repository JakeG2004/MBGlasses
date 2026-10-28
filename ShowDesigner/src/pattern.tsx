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

    constructor(patType: PatternType = PatternType.SHOW, patDur: number = 1, col1: string = "#000000", col2: string = "#ffffff") {
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

            case PatternType.DARK:
                return this.displayDark(index, onUpdate);

            default:
                console.log("other");
                return null;
        }
    }

    displayShow(index: number, onUpdate?: () => void) {
        return (
            <div key={index} className="pattern-row">
                <div className="pattern-index">{index + 1}.</div>
                {this.displayPatternType(onUpdate)}
                <span>color</span>
                {this.displayColor1(onUpdate)}
                <span>for</span>
                {this.displayDuration(onUpdate)}
            </div>
        );
    }

    displayFade(index: number, onUpdate?: () => void) {
        return (
            <div key={index} className="pattern-row">
                <div className="pattern-index">{index + 1}.</div>
                {this.displayPatternType(onUpdate)}
                <span>from color</span>
                {this.displayColor1(onUpdate)}
                <span>to</span>
                {this.displayColor2(onUpdate)}
                <span>over</span>
                {this.displayDuration(onUpdate)}
            </div>
        );
    }

    displayDark(index: number, onUpdate?: () => void) {
        return (
            <div key={index} className="pattern-row">
                <div className="pattern-index">{index + 1}.</div>
                {this.displayPatternType(onUpdate)}
                <span>for</span>
                {this.displayDuration(onUpdate)}
            </div>
        );
    }

    displayPatternType(onUpdate?: () => void) {
        return (
            <select
                className="pattern-type-select"
                value={PatternType[this.patternType]}
                onChange={e => {
                    const selectedKey = e.target.value as keyof typeof PatternType;
                    const newType = PatternType[selectedKey];
                    
                    this.patternType = newType;
                    
                    if (onUpdate) {
                        onUpdate();
                    }
                }}
            >
                <option value="SHOW">Show</option>
                <option value="FADE">Fade</option>
                <option value="DARK">Dark</option>
            </select>
        );
    }

    displayDuration(onUpdate?: () => void) {
        return (
            <div className="duration-field">
                <input 
                    className="duration-input"
                    type="number" 
                    value={this.duration || ''} 
                    onChange={e => {
                        const value = e.target.value;
                        this.duration = value === '' ? 0 : Number(value);

                        if(onUpdate) {
                            onUpdate();
                        }
                    }}
                />
                <span>second(s)</span>
            </div>
        );
    }

    displayColor1(onUpdate?: () => void) {
        return (
            <div className="color-field">
                {/* backgroundColor is dynamic, so it stays inline */}
                <div 
                    className="color-swatch"
                    onClick={(e) => {
                        e.stopPropagation();
                        this.showPicker1 = !this.showPicker1;
                        this.showPicker2 = false;
                        if (onUpdate) onUpdate();
                    }}
                    style={{ backgroundColor: this.color1 }}
                />
                {this.showPicker1 && (
                    <>
                        <div 
                            className="color-backdrop"
                            onClick={(e) => {
                                e.stopPropagation();
                                this.showPicker1 = false;
                                if (onUpdate) onUpdate();
                            }}
                        />
                        {/* Popover Window */}
                        <div className="color-popover">
                            <div className="color-popover-content">
                                <HexColorPicker 
                                    color={this.color1} 
                                    onChange={(newColor) => {
                                        this.color1 = newColor;
                                        if (onUpdate) onUpdate();
                                    }} 
                                />
                                <div className="color-hex-row">
                                    <span className="color-hex-hash">#</span>
                                    <input 
                                        className="color-hex-input"
                                        type="text" 
                                        value={this.color1.replace("#", "")} 
                                        onChange={(e) => {
                                            const sanitized = e.target.value.replace(/[^0-9A-Fa-f]/g, "").slice(0, 6);
                                            this.color1 = "#" + sanitized;
                                            if (onUpdate) onUpdate();
                                        }}
                                    />
                                </div>
                            </div>
                        </div>
                    </>
                )}
            </div>
        );
    }

    displayColor2(onUpdate?: () => void) {
        return (
            <div className="color-field">
                {/* backgroundColor is dynamic, so it stays inline */}
                <div 
                    className="color-swatch"
                    onClick={(e) => {
                        e.stopPropagation();
                        this.showPicker2 = !this.showPicker2;
                        this.showPicker1 = false;
                        if (onUpdate) onUpdate();
                    }}
                    style={{ backgroundColor: this.color2 }}
                />
                {this.showPicker2 && (
                    <>
                        <div 
                            className="color-backdrop"
                            onClick={(e) => {
                                e.stopPropagation();
                                this.showPicker2 = false;
                                if (onUpdate) onUpdate();
                            }}
                        />
                        {/* Popover Window */}
                        <div className="color-popover">
                            <div className="color-popover-content">
                                <HexColorPicker 
                                    color={this.color2} 
                                    onChange={(newColor) => {
                                        this.color2 = newColor;
                                        if (onUpdate) onUpdate();
                                    }} 
                                />
                                <div className="color-hex-row">
                                    <span className="color-hex-hash">#</span>
                                    <input 
                                        className="color-hex-input"
                                        type="text" 
                                        value={this.color2.replace("#", "")} 
                                        onChange={(e) => {
                                            const sanitized = e.target.value.replace(/[^0-9A-Fa-f]/g, "").slice(0, 6);
                                            this.color2 = "#" + sanitized;
                                            if (onUpdate) onUpdate();
                                        }}
                                    />
                                </div>
                            </div>
                        </div>
                    </>
                )}
            </div>
        );
    }
}