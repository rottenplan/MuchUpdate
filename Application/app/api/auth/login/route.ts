import { NextResponse } from 'next/server';

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const { email, password } = body;

    // --- USER PERSISTENCE LOGIC ---
    const fs = require('fs');
    const path = require('path');
    const usersFilePath = path.join(process.cwd(), 'data', 'users.json');

    let users = [];
    if (fs.existsSync(usersFilePath)) {
      users = JSON.parse(fs.readFileSync(usersFilePath, 'utf8'));
    }

    const user = users.find((u: any) =>
      (u.username === email || u.email === email) && u.password === password
    );

    if (user) {
      return NextResponse.json({
        success: true,
        token: 'auth_token_' + Date.now(),
        user: {
          id: user.id || 1,
          name: user.name || user.username,
          email: user.email || user.username
        }
      });
    }

    return NextResponse.json(
      { success: false, message: 'Invalid credentials' },
      { status: 401 }
    );
  } catch (error) {
    return NextResponse.json(
      { success: false, message: 'Internal server error' },
      { status: 500 }
    );
  }
}
