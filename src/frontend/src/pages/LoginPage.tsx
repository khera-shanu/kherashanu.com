import { useEffect } from "react"
import { useNavigate, useLocation } from "react-router-dom"
import { Button } from "@/components/ui/button"
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card"
import { useAuth } from "@/lib/auth"
import { Loader2 } from "lucide-react"

// Google OAuth configuration
const GOOGLE_CLIENT_ID = "400977528834-vlaj76rfolagur46ghkrqgap3fsdjule.apps.googleusercontent.com";
const REDIRECT_URI = `${window.location.origin}/auth`;

function getGoogleOAuthUrl() {
    const params = new URLSearchParams({
        client_id: GOOGLE_CLIENT_ID,
        redirect_uri: REDIRECT_URI,
        response_type: "code",
        scope: "openid email profile",
        access_type: "offline",
        prompt: "consent",
    });
    return `https://accounts.google.com/o/oauth2/auth?${params.toString()}`;
}

export default function LoginPage() {
    const { isLoggedIn, loading } = useAuth()
    const navigate = useNavigate()
    const location = useLocation()

    // Get the intended destination after login
    const from = (location.state as { from?: { pathname: string } })?.from?.pathname || '/admin/dashboard';

    // Redirect if already logged in
    useEffect(() => {
        if (!loading && isLoggedIn) {
            navigate(from, { replace: true });
        }
    }, [isLoggedIn, loading, navigate, from]);

    const handleGoogleLogin = () => {
        // Save intended destination for after OAuth callback
        sessionStorage.setItem('auth_redirect', from);
        // Redirect to Google OAuth
        window.location.href = getGoogleOAuthUrl();
    };

    if (loading) {
        return (
            <div className="flex items-center justify-center min-h-[80vh]">
                <Loader2 className="h-8 w-8 animate-spin" />
            </div>
        );
    }

    return (
        <div className="flex items-center justify-center min-h-[80vh] container">
            <Card className="w-full max-w-sm">
                <CardHeader className="text-center">
                    <CardTitle className="text-2xl">Admin Login</CardTitle>
                    <CardDescription>
                        Sign in with your authorized Google account to access the dashboard.
                    </CardDescription>
                </CardHeader>
                <CardContent>
                    <Button
                        className="w-full"
                        onClick={handleGoogleLogin}
                        size="lg"
                    >
                        {/* SVG omitted for brevity */}
                        <svg className="mr-2 h-4 w-4" viewBox="0 0 24 24">
                            <path
                                fill="currentColor"
                                d="M22.56 12.25c0-.78-.07-1.53-.2-2.25H12v4.26h5.92c-.26 1.37-1.04 2.53-2.21 3.31v2.77h3.57c2.08-1.92 3.28-4.74 3.28-8.09z"
                            />
                            <path
                                fill="currentColor"
                                d="M12 23c2.97 0 5.46-.98 7.28-2.66l-3.57-2.77c-.98.66-2.23 1.06-3.71 1.06-2.86 0-5.29-1.93-6.16-4.53H2.18v2.84C3.99 20.53 7.7 23 12 23z"
                            />
                            <path
                                fill="currentColor"
                                d="M5.84 14.09c-.22-.66-.35-1.36-.35-2.09s.13-1.43.35-2.09V7.07H2.18C1.43 8.55 1 10.22 1 12s.43 3.45 1.18 4.93l2.85-2.22.81-.62z"
                            />
                            <path
                                fill="currentColor"
                                d="M12 5.38c1.62 0 3.06.56 4.21 1.64l3.15-3.15C17.45 2.09 14.97 1 12 1 7.7 1 3.99 3.47 2.18 7.07l3.66 2.84c.87-2.6 3.3-4.53 6.16-4.53z"
                            />
                        </svg>
                        Sign in with Google
                    </Button>

                </CardContent>
            </Card>
        </div>
    )
}
