import { useEffect, useState } from "react"
import { useNavigate } from "react-router-dom"
import { useAuth } from "@/lib/auth"
import { Loader2 } from "lucide-react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"

export default function AuthCallbackPage() {
    const [error, setError] = useState<string | null>(null)
    const { loginWithCode } = useAuth()
    const navigate = useNavigate()

    useEffect(() => {
        const handleCallback = async () => {
            const params = new URLSearchParams(window.location.search);
            const code = params.get('code');
            const errorParam = params.get('error');

            if (errorParam) {
                setError(`Google OAuth error: ${errorParam}`);
                return;
            }

            if (!code) {
                setError('No authorization code received');
                return;
            }

            try {
                await loginWithCode(code);
                const redirectTo = sessionStorage.getItem('auth_redirect') || '/admin/dashboard';
                sessionStorage.removeItem('auth_redirect');
                navigate(redirectTo, { replace: true });
            } catch (err) {
                setError(err instanceof Error ? err.message : 'Login failed');
            }
        };

        handleCallback();
    }, [loginWithCode, navigate]);

    if (error) {
        return (
            <div className="flex items-center justify-center min-h-[80vh] container">
                <Card className="w-full max-w-sm">
                    <CardHeader className="text-center">
                        <CardTitle className="text-2xl text-destructive">Login Failed</CardTitle>
                    </CardHeader>
                    <CardContent className="text-center">
                        <p className="text-muted-foreground mb-4">{error}</p>
                        <a href="/login" className="text-primary hover:underline">
                            Try again
                        </a>
                    </CardContent>
                </Card>
            </div>
        );
    }

    return (
        <div className="flex flex-col items-center justify-center min-h-[80vh] gap-4">
            <Loader2 className="h-8 w-8 animate-spin" />
            <p className="text-muted-foreground">Completing sign in...</p>
        </div>
    );
}
