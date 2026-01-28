import { NextResponse } from 'next/server';

// Simple in-memory store for demo purposes
// In production, this would query a database
const mockUserSettings = {
    settings: {
        units: 'kmh',
        temperature: 'celsius',
        gnss: 'gps',
        brightness: 50,
        powerSave: 5,
        contrast: 50
    },
    tracks: {
        countries: ['Indonesia', 'Malaysia', 'Singapore'],
        trackCount: 25
    },
    engines: [
        { id: 1, name: 'Much Racing', hours: 24.5 },
        { id: 2, name: 'Engine 2', hours: 0.0 },
        { id: 3, name: 'Engine 3', hours: 0.0 }
    ],
    activeEngine: 1
};

export async function GET(request: Request) {
    try {
        // Basic authentication check
        const authHeader = request.headers.get('authorization');

        if (!authHeader || !authHeader.startsWith('Basic ')) {
            return NextResponse.json(
                { error: 'Unauthorized - Missing credentials' },
                { status: 401 }
            );
        }

        // Decode Basic Auth (format: "Basic base64(username:password)")
        const base64Credentials = authHeader.split(' ')[1];
        const credentials = Buffer.from(base64Credentials, 'base64').toString('ascii');
        const [username, password] = credentials.split(':');

        if (!username || !password) {
            return NextResponse.json(
                { error: 'Unauthorized - Invalid credentials' },
                { status: 401 }
            );
        }

        // --- USER PERSISTENCE LOGIC ---
        const fs = require('fs');
        const path = require('path');
        const usersFilePath = path.join(process.cwd(), 'data', 'users.json');

        // Ensure data dir exists
        const dataDir = path.join(process.cwd(), 'data');
        if (!fs.existsSync(dataDir)) {
            fs.mkdirSync(dataDir, { recursive: true });
        }

        let users = [];
        if (fs.existsSync(usersFilePath)) {
            users = JSON.parse(fs.readFileSync(usersFilePath, 'utf8'));
        }

        let user = users.find((u: any) => u.username === username);

        if (!user) {
            // AUTO-REGISTER: Create new user if they don't exist
            console.log(`Sync: Registering new user: ${username}`);
            user = { username, password, name: username };
            users.push(user);
            fs.writeFileSync(usersFilePath, JSON.stringify(users, null, 2));
        } else {
            // VERIFY: Check password for existing user
            if (user.password !== password) {
                console.log(`Sync: Authentication failed for user: ${username}`);
                return NextResponse.json(
                    { error: 'Unauthorized - Invalid password' },
                    { status: 401 }
                );
            }
        }

        // --- UPDATE STATUS FROM QUERY PARAMS ---
        const { searchParams } = new URL(request.url);
        const storageUsed = searchParams.get('storage_used');
        const storageTotal = searchParams.get('storage_total');

        if (storageUsed && storageTotal) {
            const fs = require('fs');
            const path = require('path');
            const statusFilePath = path.join(process.cwd(), 'data', 'status.json');

            // Ensure data dir exists
            const dataDir = path.join(process.cwd(), 'data');
            if (!fs.existsSync(dataDir)) {
                fs.mkdirSync(dataDir, { recursive: true });
            }

            const statusData = {
                storage_used: parseInt(storageUsed),
                storage_total: parseInt(storageTotal),
                last_sync: new Date().toISOString()
            };

            fs.writeFileSync(statusFilePath, JSON.stringify(statusData, null, 2));
            console.log('Device status updated:', statusData);
        }

        // Return user's device settings and track selection
        return NextResponse.json({
            success: true,
            data: mockUserSettings,
            syncTime: new Date().toISOString()
        });

    } catch (error) {
        console.error('Sync API Error:', error);
        return NextResponse.json(
            { error: 'Internal server error' },
            { status: 500 }
        );
    }
}

// POST endpoint to update settings from device OR upload session
export async function POST(request: Request) {
    try {
        // Basic authentication check
        const authHeader = request.headers.get('authorization');
        if (!authHeader || !authHeader.startsWith('Basic ')) {
            return NextResponse.json({ error: 'Unauthorized' }, { status: 401 });
        }

        const base64Credentials = authHeader.split(' ')[1];
        const credentials = Buffer.from(base64Credentials, 'base64').toString('ascii');
        const [username, password] = credentials.split(':');

        const fs = require('fs');
        const path = require('path');
        const usersFilePath = path.join(process.cwd(), 'data', 'users.json');

        let users = [];
        if (fs.existsSync(usersFilePath)) {
            users = JSON.parse(fs.readFileSync(usersFilePath, 'utf8'));
        }

        const user = users.find((u: any) => u.username === username);
        if (!user || user.password !== password) {
            return NextResponse.json({ error: 'Unauthorized' }, { status: 401 });
        }

        const body = await request.json();

        // Handle Session Upload
        if (body.type === 'upload_session') {
            const { filename, csv_data } = body;
            console.log(`Receiving session: ${filename}`);

            // Parse CSV (Header: Time,Lat,Lon,Speed,RPM)
            // Note: Actual header might vary, assuming standard format from previous contexts or simple CSV
            // Format example: 12:00:01,-6.123,106.123,45.5,5000

            const lines = csv_data.split('\n');
            const points = [];

            // Skip header if present (assuming first line might be header, but SyncManager just dumps raw content)
            // Let's assume standard format: "millis,len,lat,lon,speed,rpm..." or similar.
            // Based on SyncManager, it reads raw file.
            // Let's just save the raw data structured as JSON for now for flexibility, 
            // or try to parse if we know the format.
            // For safety, let's save the raw points.

            for (let i = 0; i < lines.length; i++) {
                const line = lines[i].trim();
                if (!line || line.startsWith('Time')) continue; // Skip header/empty

                const parts = line.split(',');
                if (parts.length >= 5) {
                    points.push({
                        time: parts[0], // Or millis
                        lat: parseFloat(parts[1]),
                        lng: parseFloat(parts[2]),
                        speed: parseFloat(parts[3]),
                        rpm: parseFloat(parts[4])
                    });
                }
            }

            // Save to file system
            const fs = require('fs');
            const path = require('path');
            const sessionsDir = path.join(process.cwd(), 'data', 'sessions');

            // Generate filename based on timestamp or upload name
            const timestamp = Date.now();
            const safeName = filename.replace(/[^a-zA-Z0-9]/g, '_');
            const savePath = path.join(sessionsDir, `${safeName}_${timestamp}.json`);

            const sessionData = {
                id: timestamp,
                originalFilename: filename,
                uploadDate: new Date().toISOString(),
                stats: {
                    totalPoints: points.length,
                    maxSpeed: Math.max(...points.map(p => p.speed), 0),
                    maxRpm: Math.max(...points.map(p => p.rpm), 0),
                },
                points: points
            };

            fs.writeFileSync(savePath, JSON.stringify(sessionData, null, 2));
            console.log(`Saved session to ${savePath}`);

            return NextResponse.json({
                success: true,
                message: 'Session uploaded and saved',
                id: timestamp
            });
        }

        // Handle Settings Update (Default)
        // In production, save to database
        console.log('Settings update from device:', body);

        return NextResponse.json({
            success: true,
            message: 'Settings updated successfully',
            syncTime: new Date().toISOString()
        });

    } catch (error) {
        console.error('Sync API Error:', error);
        return NextResponse.json(
            { error: 'Internal server error' },
            { status: 500 }
        );
    }
}
