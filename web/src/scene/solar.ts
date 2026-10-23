// Where the sun is over the stadium at a moment, and how dark that makes it: the lighting for each kickoff template's
// steps (a 1:00 PM tour stays in daylight; an 8:15 PM tour goes from late afternoon through sunset into night).
// The low-precision almanac formulas (good to about 0.01 deg, far better than the lighting needs), so this runs without
// Cesium and is unit tested.

const RAD = Math.PI / 180;

export interface SunPosition {
    /** Degrees above the horizon (negative below). */
    altitude: number;
    /** Degrees clockwise from north. */
    azimuth: number;
}

export function sunPosition(utcMs: number, latitudeDeg: number, longitudeDeg: number): SunPosition {
    // Days from J2000.0 (2000-01-01 12:00 UTC).
    const d = utcMs / 86400000 - 10957.5;
    const g = (357.529 + 0.98560028 * d) * RAD;
    const q = 280.459 + 0.98564736 * d;
    const lambda = (q + 1.915 * Math.sin(g) + 0.02 * Math.sin(2 * g)) * RAD;
    const e = (23.439 - 0.00000036 * d) * RAD;
    const ra = Math.atan2(Math.cos(e) * Math.sin(lambda), Math.cos(lambda));
    const dec = Math.asin(Math.sin(e) * Math.sin(lambda));
    const gmstHours = (((18.697374558 + 24.06570982441908 * d) % 24) + 24) % 24;
    const hourAngle = (gmstHours * 15 + longitudeDeg) * RAD - ra;
    const lat = latitudeDeg * RAD;
    const altitude = Math.asin(Math.sin(lat) * Math.sin(dec) + Math.cos(lat) * Math.cos(dec) * Math.cos(hourAngle));
    const azimuth = Math.atan2(-Math.sin(hourAngle), Math.tan(dec) * Math.cos(lat) - Math.sin(lat) * Math.cos(hourAngle));
    return { altitude: altitude / RAD, azimuth: (((azimuth / RAD) % 360) + 360) % 360 };
}

const smooth = (edge0: number, edge1: number, x: number) => {
    const t = Math.min(Math.max((x - edge0) / (edge1 - edge0), 0), 1);
    return t * t * (3 - 2 * t);
};

export interface DaylightLevels {
    /** 0 in daylight, 1 once civil twilight is over (the sun 6 deg or more below the horizon). */
    night: number;
    /** The warm, low-sun light of the hour before sunset (0 at night and at midday). */
    golden: number;
}

export function daylightLevels(altitudeDeg: number): DaylightLevels {
    const night = smooth(3, -6, altitudeDeg);
    const golden = smooth(16, 4, altitudeDeg) * (1 - night);
    return { night, golden };
}
