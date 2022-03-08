output "db_instance_id" {
  value = aws_db_instance.galaxy.id
}

output "db-secret-arn" {
  value = aws_secretsmanager_secret.underpass_database_credentials.arn
}

output "db-secret-version-arn" {
  value = aws_secretsmanager_secret_version.underpass_database_credentials.arn
}

output "db-security-group" {
  value = aws_security_group.database.id
}
